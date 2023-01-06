#include "histogram_iterator.h"

#include "pdu/block/index.h"

#include <boost/lexical_cast.hpp>

#include <utility>

std::pair<std::string_view, std::string_view> splitName(std::string_view name) {
    auto pos = name.rfind('_');
    if (pos == std::string_view::npos) {
        return {name, ""};
    }
    auto type = name.substr(pos + 1);
    if (!(type == "bucket" || type == "count" || type == "sum")) {
        return {name, ""};
    }

    return {name.substr(0, pos), type};
}

std::string_view getName(const Series& series) {
    return series.labels.at("__name__");
}

std::string_view baseName(const Series& series) {
    return splitName(getName(series)).first;
}

std::string_view type(const Series& series) {
    return splitName(getName(series)).second;
}

Labels canonicalise(Labels labels) {
    labels["__name__"] = splitName(labels.at("__name__")).first;
    labels.erase("le");

    // NOTE: Non-general behaviour here! For working with recording rules
    // which copy `__name__` into `name`. Handling this here constitutes
    // hidden magic and will probably surprise some users one day.
    labels.erase("name");

    return labels;
}

std::optional<HistogramTimeSpan> HistogramAccumulator::addSeries(
        const CrossIndexSeries& series) {
    const auto& labels = series.getSeries().labels;

    auto canonLabels = canonicalise(labels);

    auto res = partialHistograms.try_emplace(canonLabels);
    auto itr = res.first;

    auto& seriesSoFar = itr->second;

    auto [baseName, type] = splitName(labels.at("__name__"));
    if (type == "bucket") {
        // found a bucket, but the histogram is not yet complete, keep
        // collecting more series.
        seriesSoFar.push_back(series);
    } else if (type == "sum") {
        // found a _sum, which should always be seen after all buckets
        // of a histogram (series are encountered lexicographically ordered by
        // label key and value)
        auto histBuckets = std::move(itr->second);
        partialHistograms.erase(itr);
        if (histBuckets.empty()) {
            // this histogram has no buckets - maybe it was actually a
            // summary (which has _sum but no _bucket).
            // skip it.
            return {};
        }
        // sort by the "le" label as a double, not by the raw string value.
        std::sort(histBuckets.begin(),
                  histBuckets.end(),
                  [](const auto& a, const auto& b) {
                      return boost::lexical_cast<double>(
                                     a.getSeries().labels.at("le")) <
                             boost::lexical_cast<double>(
                                     b.getSeries().labels.at("le"));
                  });
        return {HistogramTimeSpan(canonLabels, std::move(histBuckets), series)};
    }
    return {};
}

HistogramIterator::HistogramIterator(SeriesIterator seriesIterator)
    : seriesIterator(seriesIterator) {
    increment();
}

Labels stripHistogramLabels(Labels labels) {
    labels.erase("le");
    labels.erase("__name__");

    // NOTE: Non-general behaviour here! For working with recording rules
    // which copy `__name__` into `name`. Handling this here constitutes
    // hidden magic and will probably surprise some users one day.
    labels.erase("name");
    return labels;
}

bool isSameHistogram(const Series& a, const Series& b) {
    if (baseName(a) != baseName(b)) {
        return false;
    }

    auto aLabels = stripHistogramLabels(a.labels);
    auto bLabels = stripHistogramLabels(b.labels);

    return aLabels == bLabels;
}

void HistogramIterator::increment() {
    std::vector<CrossIndexSeries> bucketSeries;
    CrossIndexSeries count;
    CrossIndexSeries sum;
    std::string_view name;
    Labels labels;

    while (seriesIterator != end(seriesIterator)) {
        auto res = acc.addSeries(*seriesIterator);
        ++seriesIterator;
        if (res) {
            hts = *res;
            return;
        }
    }
    finished = true;
}