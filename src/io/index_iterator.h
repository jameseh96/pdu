#pragma once

#include "index.h"
#include "util/generator_iterator.h"

#include <boost/filesystem.hpp>

struct IndexIterValue {
    Index index;
    boost::filesystem::path directory;
};

class IndexIterator : public generator_iterator<IndexIterator, IndexIterValue> {
public:
    IndexIterator(const boost::filesystem::path& path);

    bool next(IndexIterValue& index);

private:
    boost::filesystem::directory_iterator dirIter;
};