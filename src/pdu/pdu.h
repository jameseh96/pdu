#pragma once

#include "query/series_iterator.h"

#include <boost/filesystem.hpp>

#include <memory>
#include <vector>

class SeriesFilter;

class PrometheusData {
public:
    PrometheusData(const boost::filesystem::path& dataDir);

    SeriesIterator begin() const;
    EndSentinel end() const {
        return {};
    }

    SeriesIterator filtered(const SeriesFilter& filter) const;

private:
    std::vector<std::shared_ptr<Index>> indexes;
};

namespace pdu {
PrometheusData load(const boost::filesystem::path& path);
PrometheusData load(const std::string& path);
}