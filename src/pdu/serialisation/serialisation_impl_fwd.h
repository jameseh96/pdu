#pragma once

class Encoder;
class CrossIndexSampleIterator;
class SeriesSampleIterator;
class ChunkView;
namespace pdu::detail {
void serialise_impl(Encoder& e, const SeriesSampleIterator& ssi);
void serialise_impl(Encoder& e, const CrossIndexSampleIterator& cisi);
void serialise_impl(Encoder& e, const ChunkView& cv);
} // namespace pdu::detail