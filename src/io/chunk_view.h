#pragma once

#include "bit_decoder.h"
#include "chunk_file_cache.h"
#include "util/generator_iterator.h"

#include <limits>

class ChunkReference;

struct Sample {
    static constexpr uint16_t noBitWidth = std::numeric_limits<uint16_t>::max();

    int64_t timestamp;
    double value;
    struct {
        // The first two sample timestamps are not encoded as delta-of-deltas.
        // avoid including them in the minimal bit width breakdown.
        uint16_t minTimestampBitWidth = noBitWidth;
        uint16_t timestampBitWidth = 0;
        uint16_t valueBitWidth = 0;
    } meta;
};

struct SampleIterator : public generator_iterator<SampleIterator, Sample> {
    SampleIterator(Decoder& dec, size_t sampleCount);

    bool next(Sample& s);

private:
    double readValue();

    /// return new TS, and raw DOD
    std::pair<int64_t, int64_t> readTS();
    int64_t readTSDod();

    struct {
        int64_t ts;
        int64_t tsDelta;
        double value;
        uint8_t leading;
        uint8_t trailing;
    } prev;
    size_t currentIndex = 0;
    size_t sampleCount;
    Decoder& dec;
    BitDecoder bits;
};

// non-copying type, caller must ensure the cached chunk file handle
// outlives a ChunkView instance
class ChunkView {
public:
    ChunkView(ChunkFileCache& cfc, const ChunkReference& chunkRef);

    SampleIterator samples() {
        return {dec, sampleCount};
    }

    size_t dataLen;
    size_t dataOffset;

    size_t sampleCount;

    Decoder& dec;
};