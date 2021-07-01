#pragma once

#include "index.h"
#include "util/generator_iterator.h"

#include <boost/filesystem.hpp>

class IndexIterator : public generator_iterator<IndexIterator, Index> {
public:
    IndexIterator(const boost::filesystem::path& path);

    bool next(Index& index);

private:
    boost::filesystem::directory_iterator dirIter;
};