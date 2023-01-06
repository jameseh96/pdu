#pragma once

#include "chunk_reference.h"
#include "resource.h"
#include "series_source.h"

#include "posting_offset_iterator.h"

#include <nlohmann/json_fwd.hpp>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

class Decoder;

struct SymbolTable {
    std::vector<std::string> symbols;
    std::string_view lookup(size_t index) const;

    void load(Decoder& dec);
};

struct TOC {
    uint64_t symbol_offset;
    uint64_t series_offset;
    uint64_t label_indices_offset;
    uint64_t label_offset_table_offset;
    uint64_t postings_start_offset;
    uint64_t postings_offset_table_offset;

    void load(Decoder& dec);
};

struct Series {
    std::map<std::string_view, std::string_view> labels;
    std::vector<ChunkReference> chunks;

    using const_iterator = decltype(chunks)::const_iterator;

    auto begin() const {
        return chunks.begin();
    }

    auto end() const {
        return chunks.end();
    }

    bool empty() const {
        return chunks.empty();
    }

    void load(Decoder& dec, const SymbolTable& symbols);
};

std::ostream& operator<<(std::ostream& os, const Series& s);

// perform 3 way lexicographical compare on the labels from a pair of series
// (i.e., in the style of strcmp, or C++ 20 <=>)
int8_t compare(const Series& a, const Series& b);

inline bool operator<(const Series& a, const Series& b) {
    return compare(a, b) < 0;
}

inline bool operator>(const Series& a, const Series& b) {
    return compare(a, b) > 0;
}

struct SeriesTable {
    std::map<uint64_t, Series> series;
    using const_iterator = decltype(series)::const_iterator;

    void load(Decoder& dec, const SymbolTable& symbols, size_t expectedEnd);

    auto begin() const {
        return series.begin();
    }

    auto end() const {
        return series.end();
    }

    const auto& at(uint64_t k) const {
        return series.at(k);
    }

    auto find(uint64_t k) const {
        return series.find(k);
    }
};

struct Posting {
    Posting(Decoder dec);
    std::set<size_t> seriesReferences;
};

struct PostingOffsetTable {
    void load(Decoder dec);

    PostingOffsetIterator begin() const;
    EndSentinel end() const {
        return {};
    };

private:
    uint32_t len;
    uint32_t entries;
    Decoder offsetTableDec;
};

struct IndexMeta {
    std::string ulid;
    int64_t minTime;
    int64_t maxTime;
    struct {
        uint64_t numSamples;
        uint64_t numSeries;
        uint64_t numChunks;
    } stats;

    uint64_t version;

    struct {
        int64_t level;
        std::vector<std::string> sources;
        std::vector<std::string> parentULIDs;
    } compaction;
};

void from_json(const nlohmann::json& j, IndexMeta& meta);

class ChunkFileCache;

struct Index : public SeriesSource {
    Index() = default;

    // Index structures contain pointers into the contained symbol table
    // fixing them up in copy constructors would be error-prone, deny
    // copy/move for now.
    Index(const Index&) = delete;
    Index(Index&&) = delete;
    Index& operator=(const Index&) = delete;
    Index& operator=(Index&&) = delete;

    SymbolTable symbols;
    SeriesTable series;
    PostingOffsetTable postings;
    TOC toc;

    IndexMeta meta;

    // store mmapped chunk files on first access, as they are likely to be
    // used repeatedly.
    std::shared_ptr<ChunkFileCache> cache;

    void load(std::shared_ptr<Resource> resource);

    std::set<size_t> getSeriesRefs(const PostingOffset& offset) const {
        Posting p(resource->getDecoder().seek(offset.offset));
        return p.seriesReferences;
    }

    const std::string& getDirectory() const {
        return resource->getDirectory();
    }

    std::set<SeriesRef> getFilteredSeriesRefs(
            const SeriesFilter& filter) const override;

    const Series& getSeries(SeriesRef ref) const override;

    const std::shared_ptr<ChunkFileCache>& getCachePtr() const override;

private:
    std::shared_ptr<Resource> resource;
};

std::shared_ptr<Index> loadIndex(const std::string& fname);