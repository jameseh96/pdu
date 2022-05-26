#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>

class Encoder;

enum class ChunkType {
    Block, // persistent block chunk
    Head, // head block chunk (slightly different prologue)
    /*
     * Raw ts / value bytes. This is never generated by Prometheus, but is used
     * in this lib when reading the WAL. A raw chunk is created as a convenient
     * way of integrating in-memory data with that of disk, but it's not worth
     * the CPU time re-writing an XOR chunk as it would be on disk.
     */
    Raw
};

// Magic ID base used to point to an in-memory WAL chunk.
inline constexpr uint32_t DummyFileIdBase = 0xFF000000;

struct ChunkReference {
    uint64_t minTime;
    uint64_t maxTime;
    uint64_t fileReference;
    ChunkType type = ChunkType::Block;

    ChunkReference& operator+=(const ChunkReference& other);

    uint32_t getSegmentFileId() const;
    uint32_t getOffset() const;
};

size_t makeFileReference(uint64_t fileId, uint64_t offset);

class Decoder;
std::optional<std::pair<size_t, ChunkReference>> readHeadChunkMeta(
        Decoder& dec, uint64_t fileId);
