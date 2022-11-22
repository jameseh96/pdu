#include "pypdu_series_samples.h"

#include "pdu/block/sample.h"

// Holder for a sample iterator. May be iterated in python, or eagerly loaded
// or even dumped as raw chunks (not yet implemented)

SeriesSamples::SeriesSamples(const CrossIndexSampleIterator& cisi)
    : iterator(cisi) {
}

const CrossIndexSampleIterator& SeriesSamples::getIterator() const {
    return iterator;
}

const std::vector<Sample>& SeriesSamples::getSamples() const {
    if (loadedSamples) {
        return *loadedSamples;
    }

    loadedSamples = std::vector<Sample>();
    auto& samples = *loadedSamples;
    auto itr = iterator;
    samples.reserve(itr.getNumSamples());
    for (const auto& sample : itr) {
        samples.emplace_back(sample);
    }
    return samples;
}
