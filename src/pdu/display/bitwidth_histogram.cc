#include "bitwidth_histogram.h"

#include "display_units.h"

#include <fmt/format.h>

void BitWidthHistogram::record(uint8_t value) {
    ++values[value];
}

size_t BitWidthHistogram::totalSize() const {
    size_t total = 0;
    int index = 0;
    for (auto count : values) {
        total += index++ * count;
    }
    return total;
}

size_t BitWidthHistogram::count() const {
    size_t total = 0;
    for (auto count : values) {
        total += count;
    }
    return total;
}

BitWidthHistogram& BitWidthHistogram::operator+=(
        const BitWidthHistogram& other) {
    int index = 0;
    for (auto count : other.values) {
        values[index++] += count;
    }
    return *this;
}

void BitWidthHistogram::print(bool percent, bool human) const {
    uint64_t totalCount = 0;
    uint64_t totalSize = 0;
    int index = 0;
    for (auto count : values) {
        totalCount += count;
        totalSize += index++ * count;
    }
    fmt::print("  total size: ");
    if (human) {
        auto [scaled, unit] = format::humanReadableBytes(totalSize / 8);
        fmt::print("{:<7}", fmt::format("{}{}", scaled, unit));
    } else {
        fmt::print("{:<7}", totalSize);
    }
    fmt::print("\n");
    index = 0;
    for (auto count : values) {
        auto bits = index++;
        if (!count) {
            continue;
        }
        fmt::print("    {:>2}b: {:>10}", bits, count);
        if (percent) {
            fmt::print(" {:>7.2f}% count, {:>7.2f}% size",
                       double(count * 100) / totalCount,
                       double(bits * count * 100) / totalSize);
        }

        fmt::print("\n");
    }
}