#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

struct BitWidthHistogram {
    /**
     * Record a single value in the histogram.
     */
    void record(uint8_t value);

    /**
     * Get the total number of bits recorded.
     *
     * I.e., width*count for every bucket.
     */
    size_t totalSize() const;

    /**
     * Get the total number of values which have been recorded.
     * @return
     */
    size_t count() const;

    BitWidthHistogram& operator+=(const BitWidthHistogram& other);

    /**
     * Print this histogram to stdout.
     *
     * @param percent should buckets be percentages of the total count
     * @param human should bytes be formatted with human readable units (MB, GB,
     * ...)
     */
    void print(bool percent = false, bool human = false) const;

    std::array<uint64_t, std::numeric_limits<uint8_t>::max()> values;
};