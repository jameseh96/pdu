#include "file_map.h"

#include <boost/filesystem.hpp>

FileMap::FileMap(const boost::filesystem::path& fileName) {
    mappedFile = {fileName.c_str(), read_only};

    // Map the whole file with read permissions
    region = {mappedFile, read_only};

    // Get the address of the mapped region
    void* addr = region.get_address();
    std::size_t size = region.get_size();

    char* data = static_cast<char*>(addr);

    // point the stream at the mapped data
    stream.open({data, size});
}

std::istream& FileMap::operator*() {
    return stream;
}

std::istream& FileMap::getStream() {
    return stream;
}