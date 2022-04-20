#pragma once

#include "../io/chunk_view.h"
#include "../io/index.h"
#include "../serialisation/serialisation_impl_fwd.h"
#include "../util/iterator_facade.h"

#include <memory>

// forward decl
class ChunkFileCache;
class Encoder;

class SeriesSampleIterator
    : public iterator_facade<SeriesSampleIterator, SampleInfo> {
public:
    SeriesSampleIterator() = default;
    SeriesSampleIterator(std::shared_ptr<const Series> series,
                         std::shared_ptr<ChunkFileCache> cfc);
    SeriesSampleIterator(const SeriesSampleIterator& other);

    void increment();
    const SampleInfo& dereference() const {
        return *sampleItr;
    }

    bool is_end() const {
        return itr == series->end();
    }

    size_t getNumSamples() const;

private:
    friend void pdu::detail::serialise_impl(Encoder& e,
                                            const SeriesSampleIterator& ssi);
    // needs friendship to count up chunks
    // todo: expose chunk count and remove friendship
    friend void pdu::detail::serialise_impl(
            Encoder& e, const CrossIndexSampleIterator& cisi);
    std::shared_ptr<const Series> series;
    Series::const_iterator itr;

    std::shared_ptr<ChunkFileCache> cfc;
    ChunkView cv;
    SampleIterator sampleItr;
};