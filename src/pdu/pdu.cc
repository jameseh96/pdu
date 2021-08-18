#include "pdu.h"

#include "io/index_iterator.h"
#include "query/filtered_index_iterator.h"
#include "query/series_filter.h"

#include <algorithm>

PrometheusData::PrometheusData(const boost::filesystem::path& dataDir) {
    for (auto indexPtr : IndexIterator(dataDir)) {
        indexes.push_back(indexPtr);
    }

    std::sort(indexes.begin(), indexes.end(), [](const auto& a, const auto& b) {
        return a->meta.minTime < b->meta.minTime;
    });
}

SeriesIterator PrometheusData::begin() const {
    // iterate with no filter.
    return filtered({});
}

SeriesIterator PrometheusData::filtered(const SeriesFilter& filter) const {
    std::vector<FilteredIndexIterator> filteredIndexes;

    for (auto indexPtr : indexes) {
        filteredIndexes.emplace_back(indexPtr, filter);
    }

    return SeriesIterator(std::move(filteredIndexes));
}

namespace pdu {
PrometheusData load(const boost::filesystem::path& path) {
    return {path};
}
} // namespace pdu