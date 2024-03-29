#include <gtest/gtest.h>

#include <pdu/block/chunk_view.h>
#include <pdu/block/chunk_writer.h>
#include <pdu/block/head_chunks.h>
#include <pdu/block/wal.h>
#include <pdu/encode/decoder.h>
#include <pdu/exceptions.h>

#include <boost/filesystem.hpp>
// note, included here to work around a boost issue with env.hpp, fixed in 1.80
#include <boost/process/detail/traits/wchar_t.hpp>
#include <boost/process/env.hpp>

#include <sstream>

auto datadir() {
    static boost::filesystem::path dir = [] {
        auto val = boost::this_process::environment()["DATADIR"].to_string();
        return val.empty() ? std::string(".") : val;
    }();
    return dir;
}

class FakeHeadChunks : public HeadChunks {
public:
    using HeadChunks::HeadChunks;
    using HeadChunks::loadChunkFile;
};

class HeadChunkTest : public ::testing::Test {
public:
};

namespace detail {
template <class T, std::size_t N, std::size_t... I>
constexpr std::array<char, N> make_buffer_impl(T(&&arr)[N],
                                               std::index_sequence<I...>) {
    return {{static_cast<char&&>(arr[I])...}};
}
} // namespace detail

template <class T, std::size_t N>
constexpr std::array<char, N> make_buffer(T(&&arr)[N]) {
    return detail::make_buffer_impl(std::move(arr),
                                    std::make_index_sequence<N>{});
}

TEST_F(HeadChunkTest, PartialHeadChunk) {
    // clang-format off
    auto testChunk =
            make_buffer({
                    0x1, 0x30, 0xbc, 0x91, // Head chunk magic
                    0x1, // version 1
                    0x0, 0x0, 0x0, // padding
                    // 30 bytes, minimum meta len
                    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                    // one final byte to be _beyond_ the min meta len
                    // so it tries to parse the chunk
                    0x0});
    // clang-format on
    Decoder dec(testChunk.data(), testChunk.size());

    FakeHeadChunks chunks;
    // Finding "zeroes until the end of the file" instead of a valid head
    // chunk is seemingly an intended scenario, for a head chunk which
    // just isn't filled yet.
    EXPECT_NO_THROW(chunks.loadChunkFile(dec, 0));
}

class FakeWalLoader : public WalLoader {
public:
    using WalLoader::loadFragment;
    using WalLoader::WalLoader;
};

class WALTest : public ::testing::Test {
public:
};

TEST_F(WALTest, PartialFragment) {
    // Test to ensure fragments without an end are rejected

    // clang-format off
    auto testChunk =
            make_buffer({
                    int(RecordStart),
                    0x0, 0x1, // len
                    0x0, 0x0, 0x0, 0x0, // crc
                    0x0, // value
    });
    // clang-format on
    Decoder dec(testChunk.data(), testChunk.size());

    std::map<size_t, Series> series;
    std::set<std::string, std::less<>> symbols;
    std::map<size_t, InMemWalChunk> walChunks;

    FakeWalLoader walLoader(series, symbols, walChunks);

    for (auto lastFile : {false, true}) {
        try {
            walLoader.loadFragment(dec, lastFile);
            FAIL();
        } catch (const std::logic_error& e) {
            EXPECT_TRUE(std::string(e.what()).find("incomplete record found") !=
                        std::string::npos);
        } catch (...) {
            FAIL() << "Wrong exception thrown";
        }
    }
}

TEST_F(WALTest, MiddleFragment) {
    // Test to ensure middle-of-record fragments are correctly
    // identified

    // clang-format off
    auto testChunk =
            make_buffer({
                    int(RecordStart),
                    0x0, 0x1, // len
                    0x0, 0x0, 0x0, 0x0, // crc
                    0x3, // value, record is tombstone (currently ignored)
                    int(RecordMid),
                    0x0, 0x1, // len
                    0x0, 0x0, 0x0, 0x0, // crc
                    0x0, // value
                    int(RecordEnd),
                    0x0, 0x1, // len
                    0x0, 0x0, 0x0, 0x0, // crc
                    0x0, // value
    });
    // clang-format on
    Decoder dec(testChunk.data(), testChunk.size());

    std::map<size_t, Series> series;
    std::set<std::string, std::less<>> symbols;
    std::map<size_t, InMemWalChunk> walChunks;

    FakeWalLoader walLoader(series, symbols, walChunks);

    EXPECT_NO_THROW(
            walLoader.loadFragment(dec, false /* not last file in wal */));
}

TEST_F(WALTest, MisorderedFragmentThrows) {
    // Test to ensure middle-of-record fragments are correctly
    // identified

    // clang-format off
    auto testChunk =
            make_buffer({
                    int(RecordStart),
                    0x0, 0x1, // len
                    0x0, 0x0, 0x0, 0x0, // crc
                    0x3, // value, record is tombstone (currently ignored)
                    int(RecordFull), // incorrect to see a full record here
                    0x0, 0x1, // len
                    0x0, 0x0, 0x0, 0x0, // crc
                    0x0, // value
                    int(RecordEnd),
                    0x0, 0x1, // len
                    0x0, 0x0, 0x0, 0x0, // crc
                    0x0, // value
    });
    // clang-format on
    Decoder dec(testChunk.data(), testChunk.size());

    std::map<size_t, Series> series;
    std::set<std::string, std::less<>> symbols;
    std::map<size_t, InMemWalChunk> walChunks;

    FakeWalLoader walLoader(series, symbols, walChunks);

    try {
        walLoader.loadFragment(dec, false /* not last file in wal */);
        FAIL();
    } catch (const std::logic_error& e) {
        EXPECT_TRUE(std::string(e.what()).find(
                            "Complete fragment seen in middle") !=
                    std::string::npos);
    } catch (...) {
        FAIL() << "Wrong exception thrown";
    }
}

TEST_F(WALTest, ZeroSizeRecordStartAllowed) {
    // Test to ensure middle-of-record fragments are correctly
    // identified

    // clang-format off
    auto testChunk =
            make_buffer({
                    int(RecordStart),
                    0x0, 0x0, // len
                    0x0, 0x0, 0x0, 0x0, // crc
                    int(RecordEnd),
                    0x0, 0x1, // len
                    0x0, 0x0, 0x0, 0x0, // crc
                    0x3, // value
    });
    // clang-format on
    Decoder dec(testChunk.data(), testChunk.size());

    std::map<size_t, Series> series;
    std::set<std::string, std::less<>> symbols;
    std::map<size_t, InMemWalChunk> walChunks;

    FakeWalLoader walLoader(series, symbols, walChunks);

    EXPECT_NO_THROW(
            walLoader.loadFragment(dec, false /* not last file in wal */));
}

class EncoderTest : public ::testing::Test {
public:
};

TEST_F(EncoderTest, SS) {
    uint64_t canary = 0b110111011101;
    std::stringstream ss;
    {
        Encoder e(ss);
        BitEncoder b(e);
        b.writeBits(canary, 12);
    }
    auto s = ss.str();
    Decoder d(s);
    BitDecoder::State state;
    BitDecoder b(d, state);
    EXPECT_EQ(canary, b.readBits(12));
}

class XORChunkTest : public ::testing::Test {
public:
};

TEST_F(XORChunkTest, RoundTripSyntheticSamples) {
    std::stringstream ss;
    ss.exceptions(std::ios_base::badbit | std::ios_base::failbit |
                  std::ios_base::eofbit);

    std::vector<Sample> expectedSamples;

    {
        ChunkWriter w(ss);

        int64_t ts = 0;
        double value = 0;
        auto addSample = [&](int64_t msDelta, double vDelta) {
            ts += msDelta;
            value += vDelta;
            expectedSamples.push_back({ts, value});
            w.append({ts, value});
        };

        // start with relatively routine samples, 10s apart
        addSample(10000, 1);
        addSample(10000, 1);
        // no ts change here. Shouldn't happen, but can be encoded
        // so should be handled correctly.
        addSample(0, 1);

        // now exercise different bitwidths of timestamp delta-of-deltas
        // both positive _and_ negative (dropping the timestamp _delta_
        // back to 0 each time means the delta-of-deltas will be negative
        // with the same magnitide as the previous sample)
        addSample(1, 1);
        addSample(0, 1);
        addSample(1ull << 14, 1);
        addSample(0, 1);
        addSample(1ull << 17, 1);
        addSample(0, 1);
        addSample(1ull << 20, 1);
        addSample(0, 1);

        for (int i = 0; i < 10; i++) {
            addSample(10000, 11111);
        }

        for (int i = 0; i < 20; i++) {
            // larger changes in ts and value
            addSample(55555, 453250000 * i);
        }

        // now cover a range of timestamp deltas
        for (int i = 0; i < 1000; i++) {
            addSample(i * 10, 123);
        }
        // and decreasing, with decreasing values too
        for (int i = 1000; i > 0; i--) {
            addSample(i * 10, -123);
        }
    }

    auto chunk = ss.str();

    ChunkView view(std::make_shared<MemResource>(std::string_view(chunk)),
                   0,
                   ChunkType::XORData);

    std::vector<Sample> decodedSamples;
    decodedSamples.reserve(view.numSamples());
    for (const auto& sample : view.samples()) {
        decodedSamples.push_back(sample);
    }

    EXPECT_EQ(expectedSamples.size(), decodedSamples.size())
            << "Wrong number of samples";
    for (int i = 0; i < decodedSamples.size(); i++) {
        ASSERT_EQ(expectedSamples[i], decodedSamples[i])
                << "Failed at sample " << i << " "
                << expectedSamples[i].timestamp << " "
                << expectedSamples[i].value << " "
                << decodedSamples[i].timestamp << " "
                << decodedSamples[i].value;
    }
}
