#pragma once

#include "histogram_time_span.h"
#include "pdu/util/iterator_facade.h"

#include <optional>

using Labels = std::map<std::string_view, std::string_view>;

/**
 * Helper for incrementally building Histograms as related time series are
 * encountered.
 *
 * Time series for a single histogram are not guaranteed to be sorted
 * "next" to each other; as TS are sorted legicographically by label key and
 * value, variation in any label "after" `le` will lead to interleaved
 * TS.
 *
 * foobar_bucket{le="1", zzz="baz"}
 * foobar_bucket{le="1", zzz="qux"}
 * foobar_bucket{le="2", zzz="baz"}
 * foobar_bucket{le="2", zzz="qux"}
 */
class HistogramAccumulator {
public:
    std::optional<HistogramTimeSpan> addSeries(const CrossIndexSeries& series);

private:
    std::map<Labels, std::vector<CrossIndexSeries>> partialHistograms;
};

/**
 * Iterator exposing all histograms in a (possibly filtered) set of time series.
 */
struct HistogramIterator
    : public iterator_facade<HistogramIterator, HistogramTimeSpan> {
    HistogramIterator() = default;
    HistogramIterator(SeriesIterator);

    void increment();
    const HistogramTimeSpan& dereference() const {
        return hts;
    }

    bool is_end() const {
        return finished;
    }

private:
    SeriesIterator seriesIterator;
    HistogramAccumulator acc;
    HistogramTimeSpan hts;
    bool finished = false;
};