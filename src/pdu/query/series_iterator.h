#pragma once

#include "../util/iterator_facade.h"
#include "cross_index_sample_iterator.h"
#include "filtered_index_iterator.h"

#include <list>

struct CrossIndexSeries {
    const Series* series = nullptr;
    CrossIndexSampleIterator sampleIterator;
};

class SeriesIterator
    : public iterator_facade<SeriesIterator, CrossIndexSeries> {
public:
    SeriesIterator() = default;
    SeriesIterator(std::vector<FilteredIndexIterator> indexes);

    void increment();
    const CrossIndexSeries& dereference() const {
        return value;
    }

    bool is_end() const {
        return !value.series;
    }

private:
    std::vector<FilteredIndexIterator> indexes;
    CrossIndexSeries value;
};