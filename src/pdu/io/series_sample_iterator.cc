#include "series_sample_iterator.h"


#include "../io/chunk_file_cache.h"

SeriesSampleIterator::SeriesSampleIterator(const Series& series,
                                           ChunkFileCache& cfc)
    : series(&series), cfc(&cfc) {
    itr = series.begin();
    if (itr != series.end()) {
        cv = ChunkView(cfc, *itr);
        sampleItr = cv.samples();
    }
}

SeriesSampleIterator::SeriesSampleIterator(const SeriesSampleIterator& other) {
    series = other.series;
    itr = other.itr;
    cfc = other.cfc;
    cv = ChunkView(*cfc, *itr);
    sampleItr = cv.samples();
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
