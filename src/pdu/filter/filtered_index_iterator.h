#pragma once

#include "pdu/block/chunk_file_cache.h"
#include "pdu/block/index.h"
#include "pdu/block/series_sample_iterator.h"
#include "pdu/filter/series_filter.h"
#include "pdu/util/iterator_facade.h"

#include <memory>
#include <set>

class SeriesHandle {
public:
    SeriesHandle() = default;

    SeriesHandle(std::shared_ptr<SeriesSource> source,
                 std::shared_ptr<const Series> series);

    const Series& getSeries() const;

    std::shared_ptr<const Series> getSeriesPtr() const;

    SeriesSampleIterator getSamples() const;

    void getChunks() const;

private:
    std::shared_ptr<SeriesSource> source;
    std::shared_ptr<const Series> series;
};

class FilteredSeriesSourceIterator
    : public iterator_facade<FilteredSeriesSourceIterator, SeriesHandle> {
public:
    FilteredSeriesSourceIterator(const std::shared_ptr<SeriesSource>& source,
                                 const SeriesFilter& filter);

    FilteredSeriesSourceIterator(const FilteredSeriesSourceIterator& other);

    void increment();

    const SeriesHandle& dereference() const {
        return handle;
    }

    bool is_end() const {
        return refItr == filteredSeriesRefs.end();
    }

    std::shared_ptr<SeriesSource> getSource() const {
        return source;
    }

private:
    // update the SeriesHandle to point to the series referenced by the current
    // value of refItr
    void update();

    std::shared_ptr<const Series> getCurrentSeries() const {
        // This creates an aliasing shared ptr. It can be used to access the
        // const Series it points to, but it shares the ownership information
        // with the index.
        // This prolongs the index's life until there are no users of any
        // of its contained series.
        // This is _primarily_ to reduce the possiblity of screwing up lifetimes
        // in the Python bindings; for C++ this is relatively unnecessary as
        // it is reasonable to expect destroying a container (the index) to
        // invalidate interators and references to its contents.
        const auto& series = source->getSeries(*refItr);
        return std::shared_ptr<const Series>(source, &series);
    }

    std::shared_ptr<SeriesSource> source;
    std::set<size_t> filteredSeriesRefs;
    std::set<size_t>::const_iterator refItr;
    SeriesHandle handle;
};