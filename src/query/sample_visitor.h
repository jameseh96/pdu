#pragma once

#include "io/index.h"

#include "io/series_sample_iterator.h"
#include "query/filtered_index_iterator.h"
#include <list>
#include <vector>

// Interface for visiting every sample of every series of multiple block dirs
// (i.e., across multiple index files)
class SeriesVisitor {
public:
    virtual ~SeriesVisitor();

    virtual void visit(std::vector<FilteredIndexIterator>& indexes);

protected:
    virtual void visit(const Series& series) = 0;

    virtual void visit(const Sample& sample) = 0;
};

struct CrossIndexSeries {
    const Series* series;
    std::list<SeriesSampleIterator> sampleIterators;
};

class OrderedSeriesVisitor : public virtual SeriesVisitor {
public:
    using SeriesVisitor::visit;
    void visit(std::vector<FilteredIndexIterator>& indexes) override;

private:
    static CrossIndexSeries getNextSeries(
            std::vector<FilteredIndexIterator>& indexes);
};

