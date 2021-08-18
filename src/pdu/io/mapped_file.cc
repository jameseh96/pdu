#include "mapped_file.h"

#include <boost/filesystem.hpp>

MappedFileResource::MappedFileResource(const boost::filesystem::path& fileName)
    : directory(fileName.parent_path().string()) {
    mappedFile = {fileName.c_str(), read_only};

    // Map the whole file with read permissions
    region = {mappedFile, read_only};

    // Get the address of the mapped region
    void* addr = region.get_address();
    std::size_t size = region.get_size();

    data = {static_cast<char*>(addr), size};
}

std::shared_ptr<Resource> map_file(const std::string& fileName) {
    return std::make_shared<MappedFileResource>(fileName);
}