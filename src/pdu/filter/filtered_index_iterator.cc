#include "filtered_index_iterator.h"

#include <boost/filesystem.hpp>

SeriesHandle::SeriesHandle(std::shared_ptr<SeriesSource> source,
                           std::shared_ptr<const Series> series)
    : source(std::move(source)), series(std::move(series)){};

const Series& SeriesHandle::getSeries() const {
    return *series;
}

std::shared_ptr<const Series> SeriesHandle::getSeriesPtr() const {
    return series;
}

SeriesSampleIterator SeriesHandle::getSamples() const {
    return {series, source->getCachePtr()};
}

void SeriesHandle::getChunks() const {
    // todo
}

FilteredSeriesSourceIterator::FilteredSeriesSourceIterator(
        const std::shared_ptr<SeriesSource>& source, const SeriesFilter& filter)
    : source(source) {
    filteredSeriesRefs = source->getFilteredSeriesRefs(filter);
    refItr = filteredSeriesRefs.begin();

    update();
}

FilteredSeriesSourceIterator::FilteredSeriesSourceIterator(
        const FilteredSeriesSourceIterator& other) {
    source = other.source;
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
        handle = {source, seriesPtr};
    }
}