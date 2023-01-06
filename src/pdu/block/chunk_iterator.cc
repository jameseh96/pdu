#include "chunk_iterator.h"

void ChunkIterator::increment() {
    ++itr;

    // may need to advance to the next series for more chunks
    while (itr == currentSeries().end()) {
        series.pop_front();
        if (series.empty()) {
            return;
        }
        itr = currentSeries().begin();
    }

    updateResult();
}