#pragma once

#include <map>
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

struct SeriesTable {
    std::map<uint64_t, Series> series;

    void load(Decoder& dec, const SymbolTable& symbols, size_t expectedEnd);

    auto begin() const {
        return series.begin();
    }

    auto end() const {
        return series.end();
    }
};

struct Index {
    SymbolTable symbols;
    SeriesTable series;
    TOC toc;

    void load(Decoder& dec);
};

Index loadIndex(const std::string& fname);