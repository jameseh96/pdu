#pragma once

#include "chunk_view.h"

#include <sstream>
#include <vector>

class ChunkWriter;

class ChunkBuilder {
public:
    ChunkBuilder();
    ~ChunkBuilder();
    void append(const Sample& s);
    std::vector<ChunkView> finalise();

private:
    void flush();

    std::stringstream buffer;
    std::unique_ptr<ChunkWriter> writer;
    std::vector<ChunkView> chunks;
};
