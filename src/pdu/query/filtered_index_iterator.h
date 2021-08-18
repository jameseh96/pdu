#pragma once

#include "../io/chunk_file_cache.h"
#include "../io/index.h"
#include "../io/series_sample_iterator.h"
#include "../query/series_filter.h"
#include "../util/iterator_facade.h"

#include <memory>
#include <set>

struct SeriesHandle {
    const Series* series;
    SeriesSampleIterator sampleItr;
};

class FilteredIndexIterator
    : public iterator_facade<FilteredIndexIterator, SeriesHandle> {
public:
    FilteredIndexIterator(const std::shared_ptr<Index>& index,
                          const SeriesFilter& filter);

    FilteredIndexIterator(const FilteredIndexIterator& other);

    void increment();

    const SeriesHandle& dereference() const {
        return handle;
    }

    bool is_end() const {
        return refItr == filteredSeriesRefs.end();
    }

private:
    const Series& getCurrentSeries() const {
        return index->series.at(*refItr);
    }

    std::shared_ptr<Index> index;
    std::shared_ptr<ChunkFileCache> cache;
    std::set<size_t> filteredSeriesRefs;
    std::set<size_t>::const_iterator refItr;
    SeriesHandle handle;
};