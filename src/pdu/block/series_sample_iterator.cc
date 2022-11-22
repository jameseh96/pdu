#include "series_sample_iterator.h"

#include "pdu/block/chunk_file_cache.h"

SeriesSampleIterator::SeriesSampleIterator(
        std::shared_ptr<const Series> seriesPtr,
        std::shared_ptr<ChunkFileCache> cfc)
    : series(std::move(seriesPtr)), cfc(std::move(cfc)) {
    itr = series->begin();
    if (itr != series->end()) {
        cv = ChunkView(*this->cfc, *itr);
        sampleItr = cv.samples();
    }
}

SeriesSampleIterator::SeriesSampleIterator(const SeriesSampleIterator& other) {
    series = other.series;
    itr = other.itr;
    cfc = other.cfc;
    if (itr != series->end()) {
        cv = ChunkView(*cfc, *itr);
        sampleItr = cv.samples();
    }
}

void SeriesSampleIterator::increment() {
    ++sampleItr;
    while (sampleItr == end(sampleItr)) {
        ++itr;
        if (itr == series->end()) {
            return;
        }
        cv = ChunkView(*cfc, *itr);
        sampleItr = cv.samples();
    }
}

size_t SeriesSampleIterator::getNumSamples() const {
    if (!series) {
        throw std::runtime_error(
                "numSamples called on invalid SeriesSampleIterator");
    }
    size_t total = 0;
    for (const auto& cr : *series) {
        total += ChunkView(*cfc, cr).sampleCount;
    }

    return total;
}
