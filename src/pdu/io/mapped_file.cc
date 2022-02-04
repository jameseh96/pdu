#include "mapped_file.h"

#include <boost/filesystem.hpp>
#include <exception>

// implements the required interface for boost mapped_region to map
struct MappableFD {
    using mapping_handle_t = boost::interprocess::mapping_handle_t;
    mapping_handle_t get_mapping_handle() const {
        return {fd, false};
    }
    int fd;
};

MappedFileResource::MappedFileResource(int fd) {
    loadMappable(MappableFD{fd});
}

MappedNamedFileResource::MappedNamedFileResource(
        const boost::filesystem::path& fileName)
    : directory(fileName.parent_path().string()) {
    if (boost::filesystem::is_empty(fileName)) {
        // nothing in the file, mapping will fail.
        return;
    }
    mappedFile = {fileName.c_str(), read_only};

    loadMappable(mappedFile);
}

std::shared_ptr<Resource> try_map_fd(int fd) {
    try {
        return std::make_shared<MappedFileResource>(fd);
    } catch (const boost::interprocess::interprocess_exception& e) {
        return {};
    }
}

std::shared_ptr<Resource> map_file(const std::string& fileName) {
    return std::make_shared<MappedNamedFileResource>(fileName);
}

std::shared_ptr<Resource> map_file(const boost::filesystem::path& fileName) {
    return std::make_shared<MappedNamedFileResource>(fileName);
}