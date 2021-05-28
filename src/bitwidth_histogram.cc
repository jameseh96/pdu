#include "bitwidth_histogram.h"

#include "display_units.h"

#include <fmt/format.h>

void BitWidthHistogram::record(uint16_t value) {
    ++values[value];
}

size_t BitWidthHistogram::totalSize() const {
    size_t total = 0;
    for (const auto& [size, count] : values) {
        total += size * count;
    }
    return total;
}

size_t BitWidthHistogram::count() const {
    size_t total = 0;
    for (const auto& pair : values) {
        total += pair.second;
    }
    return total;
}

BitWidthHistogram& BitWidthHistogram::operator+=(
        const BitWidthHistogram& other) {
    for (const auto& [size, count] : other.values) {
        values[size] += count;
    }
    return *this;
}

void BitWidthHistogram::print(bool percent, bool human) const {
    size_t totalCount = 0;
    size_t totalSize = 0;

    for (const auto& [size, count] : values) {
        totalCount += count;
        totalSize += size * count;
    }
    fmt::print("  total size: ");
    if (human) {
        auto [scaled, unit] = format::humanReadableBytes(totalSize / 8);
        fmt::print("{:<7}", fmt::format("{}{}", scaled, unit));
    } else {
        fmt::print("{:<7}", totalSize);
    }
    fmt::print("\n");
    for (const auto& [bits, count] : values) {
        fmt::print("    {:>2}b: {:>10}", bits, count);
        if (percent) {
            fmt::print(" {:>7.2f}% count, {:>7.2f}% size",
                       double(count * 100) / totalCount,
                       double(bits * count * 100) / totalSize);
        }

        fmt::print("\n");
    }
}