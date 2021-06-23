#include "chunk_file_cache.h"
#include "file_map.h"

#include <fmt/format.h>
#include <stdexcept>

ChunkFileCache::ChunkFileCache(boost::filesystem::path chunkDir)
    : chunkDir(std::move(chunkDir)) {
}
Decoder& ChunkFileCache::get(uint32_t segmentId) {
    if (auto itr = cache.find(segmentId); itr != cache.end()) {
        return itr->second->get();
    }

    auto path = chunkDir / fmt::format("{:0>6}", segmentId);
    if (!boost::filesystem::is_regular_file(path)) {
        throw std::runtime_error(fmt::format(
                "Index references missing chunk file: {}\n", path.string()));
    }
    auto res = cache.try_emplace(segmentId, std::make_shared<FileMap>(path));
    auto itr = res.first;

    return itr->second->get();
}