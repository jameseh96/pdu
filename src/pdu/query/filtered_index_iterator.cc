#include "filtered_index_iterator.h"

#include <boost/filesystem.hpp>

FilteredSeriesSourceIterator::FilteredSeriesSourceIterator(
        const std::shared_ptr<SeriesSource>& source, const SeriesFilter& filter)
    : source(source) {
    // Once a chunk file reference is encountered in the index, the
    // appropriate chunk file will be mmapped and inserted into the cache
    // as they are likely to be used again.
    cache = source->getCache();

    filteredSeriesRefs = source->getFilteredSeriesRefs(filter);
    refItr = filteredSeriesRefs.begin();

    update();
}

FilteredSeriesSourceIterator::FilteredSeriesSourceIterator(
        const FilteredSeriesSourceIterator& other) {
    source = other.source;
    cache = other.cache;
    filteredSeriesRefs = other.filteredSeriesRefs;
    refItr = filteredSeriesRefs.begin();
    std::advance(refItr,
                 std::distance(other.filteredSeriesRefs.begin(), other.refItr));
    handle = other.handle;
}

void FilteredSeriesSourceIterator::increment() {
    ++refItr;
    update();
}

void FilteredSeriesSourceIterator::update() {
    if (refItr != filteredSeriesRefs.end()) {
        auto seriesPtr = getCurrentSeries();
        handle = {seriesPtr.get(), SeriesSampleIterator(seriesPtr, cache)};
    }
}