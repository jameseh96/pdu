#pragma once

#include "resource.h"

#include "posting_offset_iterator.h"

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

struct ChunkReference {
    uint64_t minTime;
    uint64_t maxTime;
    size_t fileReference;

    ChunkReference& operator+=(const ChunkReference& other);

    uint32_t getSegmentFileId() const;
    uint32_t getOffset() const;
};

struct Series {
    std::map<std::string_view, std::string_view> labels;
    std::vector<ChunkReference> chunks;

    void load(Decoder& dec, const SymbolTable& symbols);
};

inline std::ostream& operator<<(std::ostream& os, const Series& s) {
    for (const auto& [k, v] : s.labels) {
        os << "    " << k << " " << v << std::endl;
    }
    return os;
}

struct SeriesTable {
    std::map<uint64_t, Series> series;

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

struct Index {
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

    void load(std::shared_ptr<Resource> resource);

    std::set<size_t> getSeriesRefs(const PostingOffset& offset) const {
        Posting p(resource->get().seek(offset.offset));
        return p.seriesReferences;
    }

    const std::string& getDirectory() const {
        return resource->getDirectory();
    }

private:
    std::shared_ptr<Resource> resource;
};

std::shared_ptr<Index> loadIndex(const std::string& fname);