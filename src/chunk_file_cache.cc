#include "chunk_file_cache.h"

#include <fmt/format.h>
#include <stdexcept>

ChunkFileCache::ChunkFileCache(boost::filesystem::path chunkDir)
    : chunkDir(std::move(chunkDir)) {
}
std::ifstream& ChunkFileCache::get(uint32_t segmentId) {
    if (auto itr = cache.find(segmentId); itr != cache.end()) {
        return *itr->second;
    }

    auto path = chunkDir / fmt::format("{:0>6}", segmentId);
    if (!boost::filesystem::is_regular_file(path)) {
        throw std::runtime_error(fmt::format(
                "Index references missing chunk file: {}\n", path.string()));
    }
    auto& ptr = cache[segmentId];
    ptr = std::make_shared<std::ifstream>(path.string(), std::ios_base::binary);

    return *ptr;
}