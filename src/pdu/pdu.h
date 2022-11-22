#pragma once

#include "histogram/histogram_iterator.h"
#include "pdu/filter/series_iterator.h"

#include <boost/filesystem.hpp>

#include <memory>
#include <vector>

class SeriesFilter;
class HeadChunks;

class PrometheusData {
public:
    PrometheusData(const boost::filesystem::path& dataDir);

    SeriesIterator begin() const;
    EndSentinel end() const {
        return {};
    }

    SeriesIterator filtered(const SeriesFilter& filter) const;

    HistogramIterator getHistograms() const;

private:
    std::vector<std::shared_ptr<Index>> indexes;
    std::shared_ptr<HeadChunks> headChunks;
};

namespace pdu {
PrometheusData load(const boost::filesystem::path& path);
PrometheusData load(const std::string& path);
}