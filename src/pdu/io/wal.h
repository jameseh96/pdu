#pragma once

#include "decoder.h"

#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

inline constexpr size_t PageSize = 32 * 1024;

// forward decls
class ChunkReference;
class Resource;
class Series;
namespace boost::filesystem {
class path;
}

enum RecordType {
    PageEmpty = 0b0000,
    RecordFull = 0b0001,
    RecordStart = 0b0010,
    RecordMid = 0b0011,
    RecordEnd = 0b0100,
    Compressed = 0b1000
};

class InMemWalChunk {
public:
    InMemWalChunk();

    void setMinTime(int64_t ts);
    void addSample(int64_t ts, double value);

    std::pair<std::shared_ptr<Resource>, ChunkReference> makeResource() const;

    bool empty() const {
        return data.empty();
    }

private:
    std::vector<uint8_t> data;
    uint64_t minTime = 0;
    uint64_t maxTime = 0;
};

class WalLoader {
public:
    WalLoader(std::map<size_t, Series>& series,
              std::set<std::string, std::less<>>& symbols,
              std::map<size_t, InMemWalChunk>& walChunks)
        : seriesMap(series), symbols(symbols), walChunks(walChunks) {
    }
    void load(const boost::filesystem::path& file);

    void clear() {
        rawBuffer.clear();
        decompressedBuffer.clear();
        needsDecompressing = false;
    }

protected:
    void loadFile(const boost::filesystem::path& file, bool isLast = false);
    void loadFragment(Decoder& dec, bool isLastFile);
    void loadRecord(Decoder dec);
    void loadSeries(Decoder& dec);
    void loadSamples(Decoder& dec);

    /**
     * copy a provided symbol into the symbol set, and return a view
     * to the stored symbol.
     *
     * Series take string view labels, so the underlying data needs to be stored
     * in the symbol table.
     */
    std::string_view addSymbol(std::string_view sym);

    std::map<size_t, Series>& seriesMap;
    std::set<std::string, std::less<>>& symbols;
    std::map<size_t, InMemWalChunk>& walChunks;

    std::vector<uint8_t> rawBuffer;
    std::vector<uint8_t> decompressedBuffer;
    bool needsDecompressing = false;
};