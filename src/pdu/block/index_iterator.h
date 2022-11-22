#pragma once

#include "index.h"
#include "pdu/util/iterator_facade.h"

#include <boost/filesystem.hpp>

class IndexIterator
    : public iterator_facade<IndexIterator, std::shared_ptr<Index>> {
public:
    IndexIterator(const boost::filesystem::path& path);

    IndexIterator(const IndexIterator& other) : dirIter(other.dirIter) {
        advanceToValidIndex();
    }

    void increment();
    const std::shared_ptr<Index>& dereference() const {
        return index;
    }

    bool is_end() const {
        return dirIter == end(dirIter);
    }

protected:
    void advanceToValidIndex();

private:
    std::shared_ptr<Index> index = nullptr;
    boost::filesystem::path path;
    boost::filesystem::directory_iterator dirIter;
};