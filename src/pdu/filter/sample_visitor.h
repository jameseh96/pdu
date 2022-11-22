#pragma once

#include "pdu/block/index.h"

#include "cross_index_sample_iterator.h"
#include "filtered_index_iterator.h"
#include "pdu/block/series_sample_iterator.h"
#include "series_iterator.h"
#include <list>
#include <vector>

class PrometheusData;
class SeriesIterator;

// Interface for visiting every sample of every series of multiple block dirs
// (i.e., across multiple index files)
class SeriesVisitor {
public:
    virtual ~SeriesVisitor();

    virtual void visit(const std::vector<std::shared_ptr<Index>>& indexes);
    virtual void visit(std::vector<FilteredSeriesSourceIterator>& indexes);

protected:
    virtual void visit(const Series& series) = 0;

    virtual void visit(const SampleInfo& sample) = 0;
};

class OrderedSeriesVisitor : public virtual SeriesVisitor {
public:
    using SeriesVisitor::visit;
    virtual void visit(std::vector<FilteredSeriesSourceIterator>& indexes);
    virtual void visit(const PrometheusData& pd);
    virtual void visit(const SeriesIterator& itr);
};
