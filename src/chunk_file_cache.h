#pragma once

#include <boost/filesystem.hpp>
#include <fstream>
#include <map>
#include <memory>

class FileMap;

class ChunkFileCache {
public:
    ChunkFileCache(boost::filesystem::path chunkDir);
    std::istream& get(uint32_t segmentId);

private:
    const boost::filesystem::path chunkDir;
    // using copyable shared_ptr as std::map refused being constructed
    // with a non-copyable type (buggy?)
    std::map<uint32_t, std::shared_ptr<FileMap>> cache;
};