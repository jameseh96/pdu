#include <gtest/gtest.h>

#include <pdu/exceptions.h>
#include <pdu/io/decoder.h>
#include <pdu/io/head_chunks.h>

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
constexpr std::array<char, N> make_buffer_impl(T (&&arr)[N],
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
    try {
        chunks.loadChunkFile(dec, 0);
        FAIL();
    } catch (const pdu::unknown_encoding_error& e) {
    } catch (...) {
        FAIL() << "Wrong exception thrown";
    }
}
