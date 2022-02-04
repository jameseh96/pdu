#pragma once

#include "decoder.h"
#include "resource.h"

#include <boost/interprocess/file_mapping.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/iostreams/stream.hpp>
#include <string_view>

using namespace boost::interprocess;
namespace io = boost::iostreams;

namespace boost::filesystem {
class path;
}

/**
 * Container for an mmapped file.
 *
 * May be mmapped from a FD.
 */
struct MappedFileResource : public Resource {
    MappedFileResource() = default;
    MappedFileResource(int fd);

    Decoder get() const override {
        return {data};
    }
    std::string_view getView() const override {
        return {data};
    }

    const std::string& getDirectory() const override {
        throw std::runtime_error(
                "MappedFileResource::getDirectory() not implemented, may have "
                "been created from a FD");
    }

    bool empty() const override {
        return data.empty();
    }

protected:
    template <class MemoryMappable>
    void loadMappable(const MemoryMappable& mm) {
        // Map the whole file with read permissions
        region = {mm, read_only};

        // Get the address of the mapped region
        void* addr = region.get_address();
        std::size_t size = region.get_size();

        data = {static_cast<char*>(addr), size};
    }

    mapped_region region;
    std::string_view data;
};

struct MappedNamedFileResource : public MappedFileResource {
    MappedNamedFileResource(const boost::filesystem::path& fileName);

    const std::string& getDirectory() const override {
        return directory;
    }

protected:
    std::string directory;
    file_mapping mappedFile;
};

std::shared_ptr<Resource> try_map_fd(int fd);
std::shared_ptr<Resource> map_file(const std::string& fileName);
std::shared_ptr<Resource> map_file(const boost::filesystem::path& fileName);