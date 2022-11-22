#pragma once

#include "pdu/filter/cross_index_sample_iterator.h"

#include <optional>
#include <vector>

class Sample;

class SeriesSamples {
public:
    SeriesSamples(const CrossIndexSampleIterator& cisi);

    const CrossIndexSampleIterator& getIterator() const;

    const std::vector<Sample>& getSamples() const;

private:
    CrossIndexSampleIterator iterator;
    mutable std::optional<std::vector<Sample>> loadedSamples;
};
