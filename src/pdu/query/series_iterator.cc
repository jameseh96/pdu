#include "series_iterator.h"

#include <utility>

SeriesIterator::SeriesIterator(
        std::vector<FilteredSeriesSourceIterator> indexes)
    : indexes(std::move(indexes)) {
    increment();
}

void SeriesIterator::increment() {
    std::list<FilteredSeriesSourceIterator*> indexesWithSeries;

    for (auto& fi : indexes) {
        if (fi == end(fi)) {
            continue;
        }
        if (indexesWithSeries.empty()) {
            indexesWithSeries.push_back(&fi);
            continue;
        }
        auto& currSeries = *(*indexesWithSeries.front())->series;

        auto res = compare(*fi->series, currSeries);
        if (res > 0) {
            continue;
        }
        if (res < 0) {
            indexesWithSeries.clear();
        }
        indexesWithSeries.push_back(&fi);
    }

    if (indexesWithSeries.empty()) {
        value = {};
        return;
    }

    const auto* series = (*indexesWithSeries.front())->series;
    std::list<SeriesSampleIterator> sampleIterators;

    for (auto* index : indexesWithSeries) {
        sampleIterators.push_back((*index)->sampleItr);
        ++(*index);
    }

    value = {series, std::move(sampleIterators)};
}
