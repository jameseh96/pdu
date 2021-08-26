#pragma once

#include "chunk_file_cache.h"
#include "chunk_reference.h"
#include "decoder.h"
#include "index.h"
#include "resource.h"
#include "wal.h"

#include <boost/filesystem.hpp>
#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

inline constexpr uint32_t HeadChunkFileMagic = 0x0130BC91;

inline constexpr size_t HeadChunkMetaMinLen = 8 + 8 + 8 + 1 + 1 + 4;

class HeadChunks {
public:
    HeadChunks(const boost::filesystem::path& dataDir);

    // private:
    void loadChunkFile(Decoder& dec, size_t fileId);

    std::shared_ptr<ChunkFileCache> cache;

    // seriesRef to chunk references
    std::map<size_t, Series> seriesMap;
    // storage for strings referenced from the wal.
    std::set<std::string, std::less<>> symbols;
    // storage for data read from the wal
    std::map<size_t, InMemWalChunk> walChunks;
};