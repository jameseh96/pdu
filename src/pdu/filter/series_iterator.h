#pragma once

#include "cross_index_sample_iterator.h"
#include "filtered_index_iterator.h"
#include "pdu/block/chunk_iterator.h"
#include "pdu/util/iterator_facade.h"

#include <list>
#include <utility>
#include <vector>

class Encoder;

struct CrossIndexSeries {
    std::vector<std::pair<std::shared_ptr<SeriesSource>,
                          std::shared_ptr<const Series>>>
            seriesCollection;

    const Series& getSeries() const {
        if (seriesCollection.empty()) {
            throw std::logic_error(
                    "Tried to read from invalid CrossIndexSeries");
        }
        return *seriesCollection.front().second;
    }

    const auto& getLabels() const {
        return getSeries().labels;
    }

    CrossIndexSampleIterator getSamples() const;

    ChunkIterator getChunks() const;

    bool valid() const {
        return !seriesCollection.empty();
    }

    explicit operator bool() const {
        return valid();
    }
};

class SeriesIterator
    : public iterator_facade<SeriesIterator, CrossIndexSeries> {
public:
    SeriesIterator() = default;
    SeriesIterator(std::vector<FilteredSeriesSourceIterator> indexes);

    void increment();
    const CrossIndexSeries& dereference() const {
        return value;
    }

    bool is_end() const {
        return !value;
    }

private:
    std::vector<FilteredSeriesSourceIterator> indexes;
    CrossIndexSeries value;
};