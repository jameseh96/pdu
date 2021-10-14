#include "serialisation.h"

#include "../io.h"
#include "../query.h"
#include "pdu/pdu.h"

#include "../io/encoder.h"

#include <boost/io/ios_state.hpp>
#include <fmt/format.h>

#include <type_traits>

namespace pdu {
namespace detail {

void serialise_impl(Encoder& e, const ChunkReference& cr) {
    e.write_varuint(cr.minTime);
    e.write_varuint(cr.maxTime);

    e.write_int(uint8_t(cr.type));
}
void serialise_impl(Encoder& e, const ChunkView& cv) {
    // find the length of the full chunk including data and header info
    auto end = cv.dataOffset + cv.dataLen;
    size_t length = end - cv.baseOffset;

    // copy the decoder
    auto dc = cv.dec;
    // seek back to the base of the chunk
    dc.seek(cv.baseOffset);

    auto data = dc.read_view(length);

    // write length + data
    e.write_varuint(data.size());
    e.write(data);
}
void serialise_impl(Encoder& e, const SeriesSampleIterator& ssi) {
    for (const auto& chunkRef : *ssi.series) {
        serialise_impl(e, chunkRef);
        serialise_impl(e, ChunkView(*ssi.cfc, chunkRef));
    }
}

/**
 * serialise_impl all samples for the provided sample iterator.
 *
 * Does not encode any series-level info (e.g., labels)
 */
void serialise_impl(Encoder& e, const CrossIndexSampleIterator& cisi) {
    size_t chunkCount = 0;
    for (const auto& itr : cisi.subiterators) {
        chunkCount += itr.series->chunks.size();
    }

    e.write_varuint(chunkCount);
    for (const auto& itr : cisi.subiterators) {
        serialise_impl(e, itr);
    }
}

/**
 * Serialise a CrossIndexSeries, writing all data required to fully
 * reconstruct the series.
 */
void serialise_impl(Encoder& e, const CrossIndexSeries& cis) {
    const auto& labels = cis.series->labels;
    e.write_varuint(labels.size());
    for (const auto& [key, value] : labels) {
        e.write_varuint(key.size());
        e.write(key);
        e.write_varuint(value.size());
        e.write(value);
    }
    serialise_impl(e, cis.sampleIterator);
}

void serialise_impl(Encoder& e,
                    std::reference_wrapper<const CrossIndexSeries> cis) {
    serialise_impl(e, (const CrossIndexSeries&)cis);
}

/**
 * Determine how many series a Range represents.
 */
template <class SeriesIterable>
size_t getNumSeries(const SeriesIterable& series) {
    return series.size();
}

size_t getNumSeries(const SeriesIterator& itr) {
    // unfortunately, there's no faster way to count the series than
    // iterating them all
    auto itrCopy = itr;
    size_t count = 0;
    for (const auto& s : itr) {
        ++count;
    }

    return count;
}

size_t getNumSeries(const PrometheusData& pd) {
    return getNumSeries(pd.begin());
}

template <class SeriesIterable>
void serialise_impl(Encoder& e, const SeriesIterable& series) {
    e.write_varuint(getNumSeries(series));

    for (const auto& cis : series) {
        serialise_impl(e, cis);
    }
}

template void serialise_impl(Encoder&, const SeriesVector&);
template void serialise_impl(Encoder&, const SeriesRefVector&);
template void serialise_impl(Encoder&, const SeriesIterator&);
template void serialise_impl(Encoder&, const PrometheusData&);

template <typename T, typename = void>
constexpr bool is_iterable_v = false;

using std::begin, std::end;
template <typename T>
constexpr bool is_iterable_v<T,
                             std::void_t<decltype(begin(std::declval<T>())),
                                         decltype(end(std::declval<T>()))>> =
        true;

} // namespace detail

void serialise(Encoder& e, const CrossIndexSeries& series) {
    e.write_int(uint8_t(Magic::Series));
    detail::serialise_impl(e, series);
}

template <class SeriesIterable>
void serialise(Encoder& e, const SeriesIterable& series) {
    static_assert(pdu::detail::is_iterable_v<SeriesIterable>);
    e.write_int(uint8_t(Magic::SeriesGroup));
    detail::serialise_impl(e, series);
}

template void serialise(Encoder&, const SeriesVector&);
template void serialise(Encoder&, const SeriesRefVector&);
template void serialise(Encoder&, const SeriesIterator&);
template void serialise(Encoder&, const PrometheusData&);

/// Deserialisation

template <class Dec>
std::pair<ChunkReference, std::shared_ptr<Resource>> deserialise_chunk(Dec& d) {
    ChunkReference ref;
    ref.minTime = d.read_varuint();
    ref.maxTime = d.read_varuint();

    ref.type = ChunkType(d.template read_int<uint8_t>());

    auto chunkLen = d.read_varuint();
    auto res = std::make_shared<OwningMemResource>(d.read(chunkLen));

    return {ref, std::move(res)};
}

template <class Dec>
DeserialisedSeries deserialise_series(Dec& d) {
    DeserialisedSeries cis;

    auto cfc = std::make_shared<ChunkFileCache>();

    auto series = std::make_shared<Series>();

    // set up the CrossIndexSeries, pointing at the newly
    // created Series
    cis.ownedSeries = series;
    cis.series = series.get();

    cis.labelStorage = std::make_shared<std::list<std::string>>();

    // read labels
    auto numLabels = d.read_varuint();
    for (int i = 0; i < numLabels; ++i) {
        auto keySize = d.read_varuint();
        cis.labelStorage->push_back(d.read(keySize));
        auto key = std::string_view(cis.labelStorage->back());

        auto valSize = d.read_varuint();
        cis.labelStorage->push_back(d.read(valSize));
        auto val = std::string_view(cis.labelStorage->back());
        series->labels.emplace(key, val);
    }

    // read chunks
    auto numChunks = d.read_varuint();
    for (int i = 0; i < numChunks; ++i) {
        auto [ref, res] = deserialise_chunk(d);
        // store each chunk in it's own resource, at offset 0
        ref.fileReference = makeFileReference(i, 0);

        cfc->store(i, std::move(res));
        series->chunks.push_back(std::move(ref));
    }

    // we don't have multiple indexes to deal with matching up
    // series for - just the one SeriesSampleIterator pointing at
    // chunks residing in memory
    SeriesSampleIterator ssi{series, cfc};

    cis.sampleIterator = {{std::move(ssi)}};

    return cis;
}

template DeserialisedSeries deserialise_series(Decoder& decoder);
template DeserialisedSeries deserialise_series(StreamDecoder& decoder);

template <class Dec>
std::vector<DeserialisedSeries> deserialise_group(Dec& decoder) {
    std::vector<DeserialisedSeries> result;
    auto numSeries = decoder.read_varuint();
    result.reserve(numSeries);
    for (int i = 0; i < numSeries; ++i) {
        result.push_back(deserialise_series(decoder));
    }
    return result;
}

template std::vector<DeserialisedSeries> deserialise_group(Decoder& decoder);
template std::vector<DeserialisedSeries> deserialise_group(
        StreamDecoder& decoder);

template <class Dec>
SeriesOrGroup deserialise(Dec& decoder) {
    auto magic = Magic(decoder.template read_int<uint8_t>());
    switch (magic) {
    case Magic::Series:
        return {deserialise_series(decoder)};
    case Magic::SeriesGroup:
        return {deserialise_group(decoder)};
    default:
        throw std::runtime_error(
                fmt::format("Unknown magic: {:x}", uint8_t(magic)));
    }
}

template SeriesOrGroup deserialise(Decoder& decoder);
template SeriesOrGroup deserialise(StreamDecoder& decoder);

StreamIterator::StreamIterator(std::istream& stream)
    : stream(stream), d(stream) {
    read_header();
    if (!is_end()) {
        series = read_one();
    }
}

void StreamIterator::increment() {
    --numSeriesExpected;
    if (numSeriesExpected) {
        series = read_one();
    }
}

void StreamIterator::read_header() {
    auto magic = Magic(d.read_int<uint8_t>());
    switch (magic) {
    case Magic::Series:
        numSeriesExpected = 1;
        break;
    case Magic::SeriesGroup:
        numSeriesExpected = d.read_varuint();
        break;
    default:
        throw std::runtime_error(
                fmt::format("Unknown magic: {:x}", uint8_t(magic)));
    }
}

DeserialisedSeries StreamIterator::read_one() {
    boost::io::basic_ios_exception_saver saver(stream);
    stream.exceptions(std::istream::failbit | std::istream::badbit |
                      std::istream::eofbit);
    return deserialise_series(d);
}

} // namespace pdu