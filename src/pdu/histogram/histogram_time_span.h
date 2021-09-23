#pragma once

#include "../query/series_iterator.h"
#include "histogram.h"

#include <gsl/gsl-lite.hpp>
#include <vector>

class HistogramTimeSpan {
public:
    HistogramTimeSpan() = default;
    HistogramTimeSpan(std::map<std::string_view, std::string_view> labels,
                      std::vector<CrossIndexSeries> buckets,
                      CrossIndexSeries sum);

    std::string_view getName() const {
        return labels.at("__name__");
    }

    const auto& getLabels() const {
        return labels;
    }

    const auto& getBuckets() const {
        return bucketBoundaries;
    }

    size_t size() const {
        return histograms.size();
    }

    bool empty() const {
        return histograms.empty();
    }

    const TimestampedHistogram& at(gsl::index i) const {
        return histograms.at(i);
    }

private:
    std::map<std::string_view, std::string_view> labels;
    std::vector<double> bucketBoundaries;
    std::vector<TimestampedHistogram> histograms;
};
