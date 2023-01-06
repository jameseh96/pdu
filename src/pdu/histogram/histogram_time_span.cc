#include "histogram_time_span.h"

#include <boost/lexical_cast.hpp>

HistogramTimeSpan::HistogramTimeSpan(
        std::map<std::string_view, std::string_view> labels,
        std::vector<CrossIndexSeries> buckets,
        CrossIndexSeries sum)
    : labels(labels),
      bucketBoundaries(std::make_shared<std::vector<double>>()) {
    if (buckets.empty()) {
        return;
    }

    // collect up bucket boundaries
    for (auto& cis : buckets) {
        const auto& labels = cis.getSeries().labels;
        if (auto itr = labels.find("le"); itr != labels.end()) {
            try {
                auto bound = boost::lexical_cast<double>(itr->second);
                bucketBoundaries->push_back(bound);
            } catch (const boost::bad_lexical_cast& e) {
                throw std::runtime_error(
                        "Histogram bucket has invalid \"le\" :" +
                        std::string(itr->second));
            }
        }
    }

    std::vector<CrossIndexSampleIterator> allIterators;

    for (auto& bucket : buckets) {
        allIterators.push_back(bucket.getSamples());
    }
    allIterators.push_back(sum.getSamples());

    auto& sumSampleItr = allIterators.back();

    auto numSamples = sumSampleItr.getNumSamples();
    histograms.reserve(numSamples);

    auto ensureIteratorsTimeAligned = [&]() -> bool {
        if (allIterators.front() == end(allIterators.front())) {
            // a series doesn't have enough samples. It doesn't matter
            // if the other series do, we can't make a full histogram
            return false;
        }

        // Try to align all the iterators at samples for the same timestamp.
        // Pick the timestamp of the first iterator, then try to advance the
        // other iterators to the same TS.
        // If any iterators are _beyond_ this TS, update the TS and start again.
        int64_t timestamp = allIterators.front()->timestamp;

        bool consistent;
        do {
            consistent = true;
            for (auto& itr : allIterators) {
                // if any iterator is "behind", skip it forward.
                // This discards some samples, but without a complete set of
                // values at that timestamp it wasn't useful anyway
                while (itr != end(itr) && itr->timestamp < timestamp) {
                    ++itr;
                }

                if (itr == end(itr)) {
                    // whether samples had to be discarded or not, this iterator
                    // has run out of values. We can not make any more
                    // consistent histograms.
                    return false;
                }

                // if the iterator is instead _ahead_, the same situation
                // applies. a given timestamp doesn't have a complete set of
                // samples.
                if (itr->timestamp > timestamp) {
                    timestamp = itr->timestamp;
                    // some other iterators are still behind this one, loop
                    // again
                    consistent = false;
                }
            }
        } while (!consistent);
        return true;
    };

    while (sumSampleItr != end(sumSampleItr)) {
        if (!ensureIteratorsTimeAligned()) {
            // some iterator has run out, we've built all the histograms we can
            return;
        }

        std::vector<double> values;
        values.reserve(buckets.size());

        for (auto& itr : allIterators) {
            if (&itr == &sumSampleItr) {
                continue;
            }
            values.push_back(itr->value);
            ++itr;
        }

        auto timestamp = sumSampleItr->timestamp;
        auto sumValue = sumSampleItr->value;
        ++sumSampleItr;

        histograms.emplace_back(timestamp, values, bucketBoundaries, sumValue);
    }
}
