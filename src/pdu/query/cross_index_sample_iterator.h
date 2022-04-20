#pragma once

#include "../io/series_sample_iterator.h"
#include "../serialisation/serialisation_impl_fwd.h"
#include "../util/iterator_facade.h"

#include <list>

class Encoder;

class CrossIndexSampleIterator
    : public iterator_facade<CrossIndexSampleIterator, SampleInfo> {
public:
    CrossIndexSampleIterator() = default;
    CrossIndexSampleIterator(std::list<SeriesSampleIterator> subiterators);

    void increment();
    const SampleInfo& dereference() const {
        return *subiterators.front();
    }

    bool is_end() const {
        return subiterators.empty();
    }

    size_t getNumSamples() const;

private:
    friend void pdu::detail::serialise_impl(
            Encoder& e, const CrossIndexSampleIterator& cisi);
    std::list<SeriesSampleIterator> subiterators;
};