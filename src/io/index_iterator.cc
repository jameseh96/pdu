#include "index_iterator.h"

#include <boost/filesystem.hpp>

IndexIterator::IndexIterator(const boost::filesystem::path& path)
    : dirIter(path) {
    advance();
}

bool IndexIterator::next(IndexIterValue& value) {
    namespace fs = boost::filesystem;
    while (dirIter != end(dirIter)) {
        const auto& file = *dirIter;
        const auto& subdir = file.path();
        const auto& indexFile = subdir / "index";
        if (!fs::is_regular_file(indexFile)) {
            ++dirIter;
            continue;
        }

        value.index = loadIndex(indexFile.string());
        value.directory = subdir;
        ++dirIter;
        return true;
    }

    return false;
}
