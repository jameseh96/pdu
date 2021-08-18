#include "cross_index_sample_iterator.h"

CrossIndexSampleIterator::CrossIndexSampleIterator(
        std::list<SeriesSampleIterator> subiterators)
    : subiterators(subiterators) {
}

void CrossIndexSampleIterator::increment() {
    if (!subiterators.empty()) {
        ++subiterators.front();
    }
    while (!subiterators.empty() &&
           subiterators.front() == end(subiterators.front())) {
        subiterators.pop_front();
    }
}
