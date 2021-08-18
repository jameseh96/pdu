#pragma once

#include "decoder.h"
#include "resource.h"

#include <boost/filesystem.hpp>
#include <fstream>
#include <map>
#include <memory>

class FileMap;

class ChunkFileCache {
public:
    ChunkFileCache(boost::filesystem::path chunkDir);
    std::shared_ptr<Resource> get(uint32_t segmentId);

private:
    const boost::filesystem::path chunkDir;
    std::map<uint32_t, std::shared_ptr<Resource>> cache;
};