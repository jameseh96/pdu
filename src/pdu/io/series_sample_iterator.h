#pragma once

#include "../io/chunk_view.h"
#include "../io/index.h"
#include "../util/iterator_facade.h"

#include <memory>

// forward decl
class ChunkFileCache;


class SeriesSampleIterator
        : public iterator_facade<SeriesSampleIterator, Sample> {
public:
    SeriesSampleIterator() = default;
    SeriesSampleIterator(const Series& series, std::shared_ptr<ChunkFileCache> cfc);
    SeriesSampleIterator(const SeriesSampleIterator& other);

    void increment();
    const Sample& dereference() const {
        return *sampleItr;
    }

    bool is_end() const {
        return itr == series->end();
    }

private:
    const Series* series;
    Series::const_iterator itr;

    std::shared_ptr<ChunkFileCache> cfc;
    ChunkView cv;
    SampleIterator sampleItr;
};