#pragma once

#include "chunk_file_cache.h"
#include "chunk_reference.h"
#include "index.h"
#include "pdu/encode/decoder.h"
#include "resource.h"
#include "series_source.h"
#include "wal.h"

#include <boost/filesystem.hpp>
#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

inline constexpr uint32_t HeadChunkFileMagic = 0x0130BC91;

inline constexpr size_t HeadChunkMetaMinLen = 8 + 8 + 8 + 1 + 1 + 4;

class HeadChunks : public SeriesSource {
public:
    HeadChunks(const boost::filesystem::path& dataDir);

    std::set<SeriesRef> getFilteredSeriesRefs(
            const SeriesFilter& filter) const override;

    const Series& getSeries(SeriesRef ref) const override;

    const std::shared_ptr<ChunkFileCache>& getCachePtr() const override;

protected:
    // allow tests to default construct and manually load data
    HeadChunks() = default;
    void loadChunkFile(Decoder& dec, uint64_t fileId);

    std::shared_ptr<ChunkFileCache> cache;

    // seriesRef to chunk references
    std::map<size_t, Series> seriesMap;
    // storage for strings referenced from the wal.
    std::set<std::string, std::less<>> symbols;
    // storage for data read from the wal
    std::map<size_t, InMemWalChunk> walChunks;
};