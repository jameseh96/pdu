#pragma once

#include <string>
#include <tuple>

namespace format {
inline constexpr size_t KB = 1024;
inline constexpr size_t MB = KB * 1024;
inline constexpr size_t GB = MB * 1024;
inline constexpr size_t TB = GB * 1024;
inline constexpr size_t PB = TB * 1024;

inline std::pair<size_t, std::string> humanReadableBytes(size_t bytes) {
    if (bytes < KB) {
        return {bytes, "  B"};
    }
    if (bytes < MB) {
        return {bytes / KB, " KB"};
    }
    if (bytes < GB) {
        return {bytes / MB, " MB"};
    }
    if (bytes < TB) {
        return {bytes / GB, " GB"};
    }
    if (bytes < PB) {
        return {bytes / TB, " TB"};
    }

    return {bytes / PB, " PB"};
}
} // namespace format