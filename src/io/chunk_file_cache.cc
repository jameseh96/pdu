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
    auto res = cache.try_emplace(segmentId,
                                 std::make_shared<MappedFileResource>(path));
    auto itr = res.first;

    return itr->second;
}