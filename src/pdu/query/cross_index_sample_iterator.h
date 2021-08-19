#pragma once

#include "../io/series_sample_iterator.h"
#include "../util/iterator_facade.h"

#include <list>

class CrossIndexSampleIterator
    : public iterator_facade<CrossIndexSampleIterator, Sample> {
public:
    CrossIndexSampleIterator() = default;
    CrossIndexSampleIterator(std::list<SeriesSampleIterator> subiterators);

    void increment();
    const Sample& dereference() const {
        return *subiterators.front();
    }

    bool is_end() const {
        return subiterators.empty();
    }

    size_t getNumSamples() const;

private:
    std::list<SeriesSampleIterator> subiterators;
};