#pragma once

#include <cstddef>
#include <cstdint>
#include <map>

struct BitWidthHistogram {
    /**
     * Record a single value in the histogram.
     */
    void record(uint16_t value);

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

    std::map<uint16_t, uint64_t> values;
};