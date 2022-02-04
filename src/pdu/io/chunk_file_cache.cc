#include "chunk_file_cache.h"
#include "mapped_file.h"

#include <fmt/format.h>
#include <stdexcept>

ChunkFileCache::ChunkFileCache(boost::filesystem::path chunkDir)
    : chunkDir(std::move(chunkDir)) {
}
std::shared_ptr<Resource> ChunkFileCache::get(uint32_t segmentId) {
    if (auto itr = cache.find(segmentId); itr != cache.end()) {
        return itr->second;
    }

    auto path = chunkDir / fmt::format("{:0>6}", segmentId);
    if (!boost::filesystem::is_regular_file(path)) {
        throw std::runtime_error(fmt::format(
                "Index references missing chunk file: {}\n", path.string()));
    }
    auto res = cache.try_emplace(
            segmentId, std::make_shared<MappedNamedFileResource>(path));
    auto itr = res.first;

    return itr->second;
}

void ChunkFileCache::store(uint32_t segmentId, std::shared_ptr<Resource> resource) {
    if (cache.find(segmentId) != cache.end()) {
        throw std::runtime_error("ChunkFileCache: resource already exists: " +
                                 std::to_string(segmentId));
    }

    cache[segmentId] = std::move(resource);
}