#pragma once

#include "../query/series_iterator.h"

#include <list>
#include <memory>
#include <string>

class Series;

/**
 * Extension of CrossIndexSeries which owns the underlying data which
 * CrossIndexSeries expects to take a non-owning view.
 *
 * Used when deserialising a Series; something needs to own the data read
 * from the stream, but a base CrossIndexSeries relies upon PrometheusData
 * owning the backing data.
 */
class DeserialisedSeries : public CrossIndexSeries {
public:
    std::shared_ptr<const Series> ownedSeries;
    // shared ownership because a copy constructed DeserialisedSeries
    // will contain string_views into this list, so need to ensure it
    // has the same lifetime
    // todo: compact into a single buffer
    std::shared_ptr<std::list<std::string>> labelStorage;
};
