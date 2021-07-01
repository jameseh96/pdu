#pragma once

#include "index.h"
#include "util/iterator_facade.h"

#include <boost/filesystem.hpp>

class IndexIterator : public iterator_facade<IndexIterator, Index> {
public:
    IndexIterator(const boost::filesystem::path& path);

    void increment();
    const Index& dereference() const {
        return index;
    }

    bool is_end() const {
        return dirIter == end(dirIter);
    }

protected:
    void advanceToValidIndex();

private:
    Index index;
    boost::filesystem::directory_iterator dirIter;
};