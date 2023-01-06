#include "series_iterator.h"

#include <utility>

CrossIndexSampleIterator CrossIndexSeries::getSamples() const {
    std::list<SeriesSampleIterator> sampleIterators;

    for (const auto& [source, series] : seriesCollection) {
        sampleIterators.emplace_back(series, source->getCachePtr());
    }

    return {std::move(sampleIterators)};
}

ChunkIterator CrossIndexSeries::getChunks() const {
    return ChunkIterator({seriesCollection.begin(), seriesCollection.end()});
}

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
        const auto& currSeries = (*indexesWithSeries.front())->getSeries();

        auto res = compare(fi->getSeries(), currSeries);
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

    std::vector<std::pair<std::shared_ptr<SeriesSource>,
                          std::shared_ptr<const Series>>>
            seriesCollection;

    for (auto* index : indexesWithSeries) {
        seriesCollection.emplace_back(index->getSource(),
                                      (**index).getSeriesPtr());
        ++(*index);
    }

    value = {std::move(seriesCollection)};
}
