#pragma once

#include "pdu/filter/series_iterator.h"

#include <list>
#include <memory>
#include <string>

class Series;
class Resource;

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
    // If this series is constructed from a stream (socket, pipe, etc.)
    // data needs to be buffered in memory..
    // If constructed from an mmapped file, all series parsed from the file
    // can reference into the mmapped data, but use this to extend the
    // life of the mapped file resource.
    std::shared_ptr<Resource> storage;
};
