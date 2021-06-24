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
 */
struct MappedFileResource : public Resource {
    MappedFileResource(const std::string& fname);
    MappedFileResource(const boost::filesystem::path& fileName);

    Decoder get() const override {
        return {data};
    }

private:
    file_mapping mappedFile;
    mapped_region region;
    std::string_view data;
};

std::shared_ptr<Resource> map_file(const std::string& fileName);