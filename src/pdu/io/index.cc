#include "index.h"

#include "../query/series_filter.h"
#include "chunk_file_cache.h"
#include "decoder.h"
#include "mapped_file.h"

#include <boost/filesystem.hpp>
#include <nlohmann/json.hpp>

std::string_view SymbolTable::lookup(size_t index) const {
    if (index == 0) {
        throw std::domain_error("SymbolTable: 0 is an impossible index");
    }
    if (index > symbols.size()) {
        throw std::domain_error("SymbolTable: too high index");
    }
    // for some reason symbols are treated as 1-indexed. adjust accordingly.
    return symbols.at(index - 1);
}

void SymbolTable::load(Decoder& dec) {
    dec.read_int<uint32_t>(); // len
    auto numSymbols = dec.read_int<uint32_t>();

    symbols.reserve(numSymbols);

    std::string value;
    for (int i = 0; i < numSymbols; ++i) {
        auto strLen = dec.read_varuint();
        if (strLen == 0) {
            continue;
        }
        value.resize(strLen);
        dec.read(value.data(), strLen);
        symbols.push_back(value);
    }
}

void TOC::load(Decoder& dec) {
    dec.read_int_to(symbol_offset);
    dec.read_int_to(series_offset);
    dec.read_int_to(label_indices_offset);
    dec.read_int_to(label_offset_table_offset);
    dec.read_int_to(postings_start_offset);
    dec.read_int_to(postings_offset_table_offset);
}

void Series::load(Decoder& dec, const SymbolTable& symbols) {
    auto len = dec.read_varuint(); // maybe?
    auto labelCount = dec.read_varuint();

    for (int i = 0; i < labelCount; ++i) {
        auto name_id = dec.read_varuint();
        auto value_id = dec.read_varuint();
        labels.emplace(symbols.lookup(name_id), symbols.lookup(value_id));
    }

    auto chunkCount = dec.read_varuint();
    if (chunkCount == 0) {
        // this may be valid during compaction, but isn't handled here
        throw std::runtime_error("Series with no chunks isn't handled");
    }

    chunks.reserve(chunkCount);

    {
        ChunkReference first;

        first.minTime = dec.read_varint(); //// IS SIGNED
        first.maxTime = dec.read_varuint() + first.minTime;
        first.fileReference = dec.read_varuint();

        chunks.push_back(std::move(first));
    }

    for (int i = 1; i < chunkCount; ++i) {
        ChunkReference chunk;
        chunk.minTime = dec.read_varint(); /// IS SIGNED
        chunk.maxTime = dec.read_varuint();
        chunk.fileReference = dec.read_varint(); /// IS SIGNED

        chunk += chunks.back();
        chunks.push_back(std::move(chunk));
    }

    dec.read_int<uint32_t>(); // CRC
}

void SeriesTable::load(Decoder& dec,
                       const SymbolTable& symbols,
                       size_t expectedEnd) {
    while (dec.consume_to_alignment(16) < expectedEnd) {
        auto offset = dec.tell();
        size_t id = offset / 16;
        Series s;
        s.load(dec, symbols);
        series[id] = std::move(s);
    }
}

void from_json(const nlohmann::json& j, IndexMeta& meta) {
    j.at("ulid").get_to(meta.ulid);
    j.at("minTime").get_to(meta.minTime);
    j.at("maxTime").get_to(meta.maxTime);
    auto stats = j.at("stats");
    stats.at("numSamples").get_to(meta.stats.numSamples);
    stats.at("numSeries").get_to(meta.stats.numSeries);
    stats.at("numChunks").get_to(meta.stats.numChunks);
}

void Index::load(std::shared_ptr<Resource> res) {
    resource = std::move(res);

    namespace fs = boost::filesystem;
    fs::path subdir = getDirectory();

    // Once a chunk file reference is encountered in the index, the
    // appropriate chunk file will be mmapped and inserted into the cache
    // as they are likely to be used again.
    cache = std::make_shared<ChunkFileCache>(subdir / "chunks");

    auto metaPath = fs::path(subdir) / "meta.json";

    if (!fs::exists(metaPath)) {
        throw std::invalid_argument(
                "Provided index directory: " + resource->getDirectory() +
                " does not contain a meta.json file");
    }

    {
        std::ifstream metaF(metaPath.string(), std::ios::in);
        meta = nlohmann::json::parse(metaF);
        metaF.close();
    }

    Decoder dec(resource->get());
    dec.seek(-(8 * 6 + 4), std::ios::end);

    toc.load(dec);

    if (!toc.symbol_offset) {
        throw std::runtime_error("No symbol table in index file");
    }

    dec.seek(toc.symbol_offset);
    symbols.load(dec);

    if (!toc.series_offset) {
        throw std::runtime_error("No series in index file");
    }

    dec.seek(toc.series_offset);
    series.load(dec, symbols, toc.label_indices_offset);

    if (!toc.postings_offset_table_offset) {
        throw std::runtime_error("No posting offset table in index file");
    }

    dec.seek(toc.postings_offset_table_offset);
    postings.load(dec);
}

std::set<SeriesSource::SeriesRef> Index::getFilteredSeriesRefs(const SeriesFilter& filter) const {
    return filter(*this);
}

const Series& Index::getSeries(SeriesRef ref) const {
    return series.at(ref);
}

std::shared_ptr<ChunkFileCache> Index::getCache() const {
    return cache;
}

Posting::Posting(Decoder dec) {
    auto len = dec.read_int<uint32_t>();
    auto entries = dec.read_int<uint32_t>();
    for (int i = 0; i < entries; ++i) {
        seriesReferences.insert(dec.read_int<uint32_t>());
    }
}

void PostingOffsetTable::load(Decoder dec) {
    dec.read_int_to(len);
    dec.read_int_to(entries);
    // postings are lazily loaded.
    offsetTableDec = dec;
}

PostingOffsetIterator PostingOffsetTable::begin() const {
    return {offsetTableDec, entries};
}

std::shared_ptr<Index> loadIndex(const std::string& fname) {
    auto resource = map_file(fname);

    auto index = std::make_shared<Index>();
    index->load(resource);
    return index;
}