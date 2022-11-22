#pragma once

#include <cstdint>
#include <limits>

struct Sample {
    int64_t timestamp;
    double value;
};

bool operator==(const Sample& a, const Sample& b);
bool operator!=(const Sample& a, const Sample& b);

struct SampleInfo : public Sample {
    static constexpr uint16_t noBitWidth = std::numeric_limits<uint16_t>::max();
    struct {
        // The first two sample timestamps are not encoded as delta-of-deltas.
        // avoid including them in the minimal bit width breakdown.
        uint16_t minTimestampBitWidth = noBitWidth;
        uint16_t timestampBitWidth = 0;
        uint16_t valueBitWidth = 0;
    } meta;
};
