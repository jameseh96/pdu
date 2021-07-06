#include "index_iterator.h"

#include <boost/filesystem.hpp>

IndexIterator::IndexIterator(const boost::filesystem::path& path)
    : path(path), dirIter(this->path) {
    advanceToValidIndex();
}

void IndexIterator::increment() {
    ++dirIter;
    advanceToValidIndex();
}

void IndexIterator::advanceToValidIndex() {
    namespace fs = boost::filesystem;
    while (dirIter != end(dirIter)) {
        const auto& file = *dirIter;
        const auto& subdir = file.path();
        const auto& indexFile = subdir / "index";
        if (fs::is_regular_file(indexFile)) {
            index = loadIndex(indexFile.string());
            break;
        }

        ++dirIter;
    }
}
