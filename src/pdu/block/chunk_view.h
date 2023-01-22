#pragma once

#include "chunk_file_cache.h"
#include "pdu/encode/bit_decoder.h"
#include "pdu/serialisation/serialisation_impl_fwd.h"
#include "pdu/util/iterator_facade.h"

#include "chunk_reference.h"
#include "sample.h"

class Encoder;

struct SampleIterator : public iterator_facade<SampleIterator, SampleInfo> {
    SampleIterator() = default;
    SampleIterator(Decoder dec, size_t sampleCount, bool rawChunk = false);

    void increment();
    const SampleInfo& dereference() const {
        return s;
    }

    bool is_end() const {
        return currentIndex == sampleCount;
    }

private:
    double readValue(BitDecoder& bits);
    /// return new TS, and raw DOD
    std::pair<int64_t, int64_t> readTS(BitDecoder& bits);
    int64_t readTSDod(BitDecoder& bits);

    struct {
        int64_t ts = 0;
        int64_t tsDelta = 0;
        double value = 0;
        uint8_t leading = 0;
        uint8_t trailing = 0;
    } prev;
    ssize_t currentIndex = -1;
    size_t sampleCount;
    Decoder dec;
    BitDecoder::State bitState;

    bool rawChunk = false;

    SampleInfo s;
};

// non-copying type. Holds a shared_ptr to the resource to ensure it
// lives as long as it is being used.
class ChunkView {
public:
    ChunkView() = default;
    ChunkView(ChunkFileCache& cfc, const ChunkReference& chunkRef);
    ChunkView(std::shared_ptr<Resource> res,
              size_t offset = 0,
              ChunkType type = ChunkType::Block);

    SampleIterator samples() const;

    size_t numSamples() const {
        return sampleCount;
    }

    operator bool() const {
        return bool(res);
    }

    bool isXOR() const {
        return !rawChunk;
    }

    std::string_view data() const;

    std::string_view xor_data() const;

    size_t dataLen;
    size_t dataOffset;

    size_t sampleCount;

private:
    friend void pdu::detail::serialise_impl(Encoder& e, const ChunkView& cv);
    // offset into the resource to the chunk start
    size_t chunkOffset;
    std::shared_ptr<Resource> res;
    bool rawChunk = false;
};