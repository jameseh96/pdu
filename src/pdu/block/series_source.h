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

    virtual const std::shared_ptr<ChunkFileCache>& getCachePtr() const = 0;

    ChunkFileCache& getCache() const {
        return *getCachePtr();
    }
};