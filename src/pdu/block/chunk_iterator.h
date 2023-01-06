#pragma once

#include "pdu/util/iterator_facade.h"

#include "chunk_view.h"
#include "index.h"

#include <deque>
#include <utility>

class SeriesSource;

using ChunkRefAndView = std::pair<ChunkReference, ChunkView>;

class ChunkIterator : public iterator_facade<ChunkIterator, ChunkRefAndView> {
public:
    ChunkIterator() = default;

    using SeriesDeque = std::deque<std::pair<std::shared_ptr<SeriesSource>,
                                             std::shared_ptr<const Series>>>;
    ChunkIterator(SeriesDeque series) : series(series) {
        // may need skip over empty chunks
        while (!series.empty()) {
            itr = currentSeries().begin();
            if (itr != currentSeries().end()) {
                updateResult();
                break;
            }
            series.pop_front();
        }
    }

    void increment();
    const auto& dereference() const {
        return refAndView;
    }

    bool is_end() const {
        return series.empty();
    }

private:
    const Series& currentSeries() const {
        return *series.front().second;
    }

    void updateResult() {
        refAndView = {*itr, {series.front().first->getCache(), *itr}};
    }

    SeriesDeque series;
    Series::const_iterator itr;
    ChunkRefAndView refAndView;
};