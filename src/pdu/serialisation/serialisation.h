#pragma once

#include "deserialised_cross_index_series.h"
#include "pdu/util/iterator_facade.h"

// using boost variant to allow targeting older MacOS before std::visit
// was available.
#include <boost/variant.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>

// Forward declarations
class Encoder;

class ChunkReference;
class ChunkView;
class SeriesSampleIterator;
class CrossIndexSampleIterator;
class CrossIndexSeries;
class SeriesIterator;
class PrometheusData;

namespace pdu {

enum class Magic : uint8_t {
    Series = 0x5A,
    SeriesGroup = 0x5B,
};

// Serialisation

using SeriesVector = std::vector<CrossIndexSeries>;
using SeriesRefVector =
        std::vector<std::reference_wrapper<const CrossIndexSeries>>;

/**
 * Serialise a series, writing a magic value allowing a reader to determine
 * ahead of time that a single series was written, rather than multiple.
 */
void serialise(Encoder& e, const CrossIndexSeries& series);

/**
 * Serialise a Range of series, writing a magic value allowing a reader to
 * determine that multiple series were written, and how many.
 */
template <class SeriesIterable>
void serialise(Encoder& e, const SeriesIterable& series);

extern template void serialise(Encoder&, const SeriesVector&);
extern template void serialise(Encoder&, const SeriesRefVector&);
extern template void serialise(Encoder&, const SeriesIterator&);
extern template void serialise(Encoder&, const PrometheusData&);

// Deserialisation

template <class Dec>
DeserialisedSeries deserialise_series(Dec& decoder);

extern template DeserialisedSeries deserialise_series(Decoder& decoder);
extern template DeserialisedSeries deserialise_series(StreamDecoder& decoder);

template <class Dec>
std::vector<DeserialisedSeries> deserialise_group(Dec& decoder);

extern template std::vector<DeserialisedSeries> deserialise_group(
        Decoder& decoder);
extern template std::vector<DeserialisedSeries> deserialise_group(
        StreamDecoder& decoder);

using SeriesOrGroup =
        boost::variant<DeserialisedSeries, std::vector<DeserialisedSeries>>;

template <class Dec>
SeriesOrGroup deserialise(Dec& decoder);

extern template SeriesOrGroup deserialise(Decoder& decoder);
extern template SeriesOrGroup deserialise(StreamDecoder& decoder);

// Overload taking a resource. The underlying data is already in memory.
// Decode it, and ensure all series reference the resource.
SeriesOrGroup deserialise(std::shared_ptr<Resource> resource);

struct StreamIterator
    : public iterator_facade<StreamIterator, DeserialisedSeries> {
public:
    StreamIterator(std::istream& stream);

    void increment();
    const DeserialisedSeries& dereference() const {
        return series;
    }

    bool is_end() const {
        return numSeriesExpected == 0;
    }

private:
    void read_header();

    DeserialisedSeries read_one();

    std::istream& stream;
    StreamDecoder d;
    DeserialisedSeries series;
    size_t numSeriesExpected = 0;
};
} // namespace pdu
