#include "head_chunks.h"

#include "../query/series_filter.h"
#include "mapped_file.h"

HeadChunks::HeadChunks(const boost::filesystem::path& dataDir) {
    auto headChunksDir = dataDir / "chunks_head";
    namespace fs = boost::filesystem;
    if (!fs::exists(headChunksDir) || !fs::exists(dataDir / "wal")) {
        return;
    }

    cache = std::make_shared<ChunkFileCache>(headChunksDir);


    std::vector<std::string> chunkFileNames;

    for (const auto& chunkFile : fs::directory_iterator(headChunksDir)) {
        chunkFileNames.emplace_back(chunkFile.path().filename().string());
    }

    std::sort(chunkFileNames.begin(), chunkFileNames.end());

    for (int i = 0; i < chunkFileNames.size(); ++i) {
        const auto& filename = chunkFileNames[i];
        uint64_t fileId;
        try {
            fileId = std::stoull(filename);
        } catch (const std::exception&) {
            throw std::runtime_error("Head chunk has unexpected filename: " +
                                     filename);
        }
        auto fileResource = cache->get(fileId);
        auto dec = fileResource->get();

        try {
            loadChunkFile(dec, fileId);
        } catch (const std::runtime_error& e) {
            // the last chunk file may be partially constructed
            // all others are expected to be fully valid
            // TODO: CRCs should be checked
            if (i != chunkFileNames.size() - 1) {
                throw;
            }

        }
    }

    // WAL
    WalLoader wl(seriesMap, symbols, walChunks);
    wl.load(dataDir);

    // now the wal has been loaded, stick the fake chunks into the cache
    // and add references to them to the series.

    int counter = 0;
    for (const auto& [ref, memchunk] : walChunks) {
        if (memchunk.empty()) {
            // if no samples were loaded from a given wal chunk,
            // ignore it.
            continue;
        }
        auto [resource, chunkref] = memchunk.makeResource();

        auto fileId = DummyFileIdBase + counter;
        chunkref.fileReference = makeFileReference(fileId, 0);

        cache->store(fileId, std::move(resource));
        seriesMap[ref].chunks.push_back(chunkref);
        ++counter;
    }
}

std::set<SeriesSource::SeriesRef> HeadChunks::getFilteredSeriesRefs(
        const SeriesFilter& filter) const {
    std::set<SeriesSource::SeriesRef> res;

    for (const auto& [ref, series] : seriesMap) {
        if (filter(series)) {
            res.insert(ref);
        }
    }
    return res;
}

const Series& HeadChunks::getSeries(SeriesRef ref) const {
    return seriesMap.at(ref);
}

std::shared_ptr<ChunkFileCache> HeadChunks::getCache() const {
    return cache;
}

void HeadChunks::loadChunkFile(Decoder& dec, uint64_t fileId) {
    auto magic = dec.read_int<uint32_t>();

    if (magic != HeadChunkFileMagic) {
        throw std::runtime_error("Head chunk file has unexpected magic: " +
                                 std::to_string(magic));
    }

    auto version = dec.read_int<uint8_t>();

    if (version != 1) {
        throw std::runtime_error("Head chunk file has unexpected version: " +
                                 std::to_string(version));
    }

    // padding
    dec.read_int<uint8_t>();
    dec.read_int<uint8_t>();
    dec.read_int<uint8_t>();

    while (dec.remaining() > HeadChunkMetaMinLen) {
        auto res = readHeadChunkMeta(dec, fileId);
        if (!res) {
            // found zeroes, assume rest of the file is empty
            break;
        }
        auto [seriesRef, chunkRef] = *res;
        seriesMap[seriesRef].chunks.push_back(std::move(chunkRef));
    }
}