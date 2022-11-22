#pragma once

#include <cstdint>
#include <memory>
#include <set>

class SeriesFilter;
class ChunkFileCache;
class Series;

class SeriesSource {
public:
    using SeriesRef = size_t;
    virtual std::set<SeriesRef> getFilteredSeriesRefs(
            const SeriesFilter& filter) const = 0;

    virtual const Series& getSeries(SeriesRef ref) const = 0;

    virtual std::shared_ptr<ChunkFileCache> getCache() const = 0;
};