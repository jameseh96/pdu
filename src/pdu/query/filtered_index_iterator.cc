#include "filtered_index_iterator.h"

#include <boost/filesystem.hpp>

FilteredIndexIterator::FilteredIndexIterator(
        const std::shared_ptr<Index>& index, const SeriesFilter& filter)
    : index(index) {
    namespace fs = boost::filesystem;
    fs::path subdir = index->getDirectory();

    // Once a chunk file reference is encountered in the index, the
    // appropriate chunk file will be mmapped and inserted into the cache
    // as they are likely to be used again.
    cache = std::make_shared<ChunkFileCache>(subdir / "chunks");

    filteredSeriesRefs = filter(*index);
    refItr = filteredSeriesRefs.begin();
    if (refItr != filteredSeriesRefs.end()) {
        const auto& series = getCurrentSeries();
        handle = {&series, SeriesSampleIterator(series, *cache)};
    }
}

FilteredIndexIterator::FilteredIndexIterator(
        const FilteredIndexIterator& other) {
    index = other.index;
    cache = other.cache;
    filteredSeriesRefs = other.filteredSeriesRefs;
    refItr = filteredSeriesRefs.begin();
    std::advance(refItr,
                 std::distance(other.filteredSeriesRefs.begin(), other.refItr));
    handle = other.handle;
}

void FilteredIndexIterator::increment() {
    ++refItr;
    if (refItr != filteredSeriesRefs.end()) {
        const auto& series = getCurrentSeries();
        handle = {&series, SeriesSampleIterator(series, *cache)};
    }
}