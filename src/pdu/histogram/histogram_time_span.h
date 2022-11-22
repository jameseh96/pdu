#pragma once

#include "histogram.h"
#include "pdu/filter/series_iterator.h"

#include <gsl/gsl-lite.hpp>
#include <memory>
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

    const auto& getBounds() const {
        return *bucketBoundaries;
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
    // shared so each timestamped histogram can extend the life of the bounds.
    // The bounds are constant over time, so don't need to be unique per
    // timestamped histogram.
    std::shared_ptr<std::vector<double>> bucketBoundaries;
    std::vector<TimestampedHistogram> histograms;
};
