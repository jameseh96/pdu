#include <gtest/gtest.h>

#include <pdu/exceptions.h>
#include <pdu/io/decoder.h>
#include <pdu/io/head_chunks.h>
#include <pdu/io/wal.h>

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
