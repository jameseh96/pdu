#include "chunk_builder.h"

#include "chunk_writer.h"
#include "resource.h"

#include <limits>

ChunkBuilder::ChunkBuilder() : writer(std::make_unique<ChunkWriter>(buffer)) {
}

ChunkBuilder::~ChunkBuilder() = default;

void ChunkBuilder::append(const Sample& s) {
    if (writer->full()) {
        flush();
    }
    writer->append(s);
}

std::vector<ChunkView> ChunkBuilder::finalise() {
    if (!writer->empty()) {
        flush();
    }
    return std::move(chunks);
}

void ChunkBuilder::flush() {
    writer->close();
    chunks.emplace_back(std::make_shared<OwningMemResource>(buffer.str()),
                        0,
                        ChunkType::XORData);
    // clear the buffer
    buffer = {};
    writer = std::make_unique<ChunkWriter>(buffer);
}