#include "sample_visitor.h"

SeriesVisitor::~SeriesVisitor() = default;

void SeriesVisitor::visit(std::vector<FilteredIndexIterator>& indexes) {
    for (const auto& fi : indexes) {
        for (const auto& [series, samples] : fi) {
            visit(*series);
            for (const auto& sample : samples) {
                visit(sample);
            }
        }
    }
}

void OrderedSeriesVisitor::visit(std::vector<FilteredIndexIterator>& indexes) {
    while (true) {
        // same series across multiple indexes
        auto seriesGroup = getNextSeries(indexes);
        if (!seriesGroup.series) {
            break;
        }
        visit(*seriesGroup.series);
        for (auto& sampleItr : seriesGroup.sampleIterators) {
            for (const auto& sample : sampleItr) {
                visit(sample);
            }
        }
    }
}

CrossIndexSeries OrderedSeriesVisitor::getNextSeries(
        std::vector<FilteredIndexIterator>& indexes) {
    std::list<FilteredIndexIterator*> indexesWithSeries;

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
        return {};
    }

    const auto* series = (*indexesWithSeries.front())->series;
    std::list<SeriesSampleIterator> sampleIterators;

    for (auto* index : indexesWithSeries) {
        sampleIterators.push_back((*index)->sampleItr);
        ++(*index);
    }

    return {series, std::move(sampleIterators)};
}
