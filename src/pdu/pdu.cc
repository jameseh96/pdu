#include "pdu.h"

#include "pdu/block/head_chunks.h"
#include "pdu/block/index_iterator.h"
#include "pdu/filter/filtered_index_iterator.h"
#include "pdu/filter/series_filter.h"

#include <algorithm>

PrometheusData::PrometheusData(const boost::filesystem::path& dataDir) {
    std::set<std::string> obsoleteHeads;
    std::set<std::string> obsoleteBlocks;
    for (auto indexPtr : IndexIterator(dataDir)) {
        const auto& parents = indexPtr->meta.compaction.parentULIDs;
        obsoleteBlocks.insert(parents.begin(), parents.end());
        indexes.push_back(indexPtr);
    }

    // remove blocks which have already been superseded by a compacted block
    indexes.erase(std::remove_if(indexes.begin(),
                                 indexes.end(),
                                 [obsoleteBlocks](const auto& indexPtr) {
                                     return obsoleteBlocks.count(
                                                    indexPtr->meta.ulid) > 0;
                                 }),
                  indexes.end());

    headChunks = std::make_shared<HeadChunks>(dataDir);

    std::sort(indexes.begin(), indexes.end(), [](const auto& a, const auto& b) {
        return a->meta.minTime < b->meta.minTime;
    });
}

SeriesIterator PrometheusData::begin() const {
    // iterate with no filter.
    return filtered({});
}

SeriesIterator PrometheusData::filtered(const SeriesFilter& filter) const {
    std::vector<FilteredSeriesSourceIterator> filteredIndexes;

    for (auto indexPtr : indexes) {
        filteredIndexes.emplace_back(indexPtr, filter);
    }

    filteredIndexes.emplace_back(headChunks, filter);

    return SeriesIterator(std::move(filteredIndexes));
}

HistogramIterator PrometheusData::getHistograms() const {
    SeriesFilter filter;
    filter.addFilter("__name__", pdu::filter::regex(".*(_bucket|_sum)"));
    return HistogramIterator(filtered(filter));
}

namespace pdu {
PrometheusData load(const boost::filesystem::path& path) {
    return {path};
}
PrometheusData load(const std::string& path) {
    return {path};
}
} // namespace pdu