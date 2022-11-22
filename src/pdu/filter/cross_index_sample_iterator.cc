#include "cross_index_sample_iterator.h"

CrossIndexSampleIterator::CrossIndexSampleIterator(
        std::list<SeriesSampleIterator> subiters)
    : subiterators(subiters) {
    while (!subiterators.empty() &&
           subiterators.front() == end(subiterators.front())) {
        subiterators.pop_front();
    }
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

size_t CrossIndexSampleIterator::getNumSamples() const {
    size_t total = 0;
    for (const auto& sub : subiterators) {
        total += sub.getNumSamples();
    }
    return total;
}
