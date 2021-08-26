#include "sample_visitor.h"

#include "../pdu.h"
#include "series_iterator.h"

SeriesVisitor::~SeriesVisitor() = default;

void SeriesVisitor::visit(const std::vector<std::shared_ptr<Index>>& indexes) {
    std::vector<FilteredSeriesSourceIterator> filteredIndexes;

    for (const auto& indexPtr : indexes) {
        filteredIndexes.emplace_back(indexPtr, SeriesFilter());
    }

    visit(filteredIndexes);
}

void SeriesVisitor::visit(std::vector<FilteredSeriesSourceIterator>& indexes) {
    for (const auto& fi : indexes) {
        for (const auto& [series, samples] : fi) {
            visit(*series);
            for (const auto& sample : samples) {
                visit(sample);
            }
        }
    }
}

void OrderedSeriesVisitor::visit(
        std::vector<FilteredSeriesSourceIterator>& indexes) {
    visit(SeriesIterator(indexes));
}

void OrderedSeriesVisitor::visit(const PrometheusData& pd) {
    visit(pd.begin());
}
void OrderedSeriesVisitor::visit(const SeriesIterator& itr) {
    for (const auto& series : itr) {
        // same series across multiple indexes
        visit(*series.series);
        for (const auto& sample : series.sampleIterator) {
            visit(sample);
        }
    }
}