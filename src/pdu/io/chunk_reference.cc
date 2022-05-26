#include "chunk_reference.h"

#include "decoder.h"

#include "../exceptions.h"

#include <utility>

ChunkReference& ChunkReference::operator+=(const ChunkReference& other) {
    minTime += other.minTime;
    maxTime += other.maxTime;
    fileReference += other.fileReference;
    return *this;
}

uint32_t ChunkReference::getSegmentFileId() const {
    return uint32_t(fileReference >> 32) + 1;
}
uint32_t ChunkReference::getOffset() const {
    return fileReference & 0xFFFFFFFF;
}

size_t makeFileReference(uint64_t fileId, uint64_t offset) {
    return ((fileId - 1) << 32) | offset;
}

std::optional<std::pair<size_t, ChunkReference>> readHeadChunkMeta(
        Decoder& dec, uint64_t fileId) {
    ChunkReference ref;
    ref.type = ChunkType::Head;

    size_t offset = dec.tell();
    ref.fileReference = makeFileReference(fileId, offset);

    size_t seriesRef = dec.read_int<uint64_t>();
    ref.minTime = dec.read_int<uint64_t>();
    ref.maxTime = dec.read_int<uint64_t>();

    auto encoding = dec.read_int<uint8_t>();
    if (encoding != 1) {
        if (encoding == 0 && ref.minTime == 0 && ref.maxTime == 0) {
            // assume there are no more chunks in this file, which is
            // an expected scenario
            return {};
        }
        throw pdu::unknown_encoding_error(
                "Head chunk meta has unknown encoding: " +
                std::to_string(encoding));
    }

    size_t dataLen = dec.read_varuint();

    // skip data and 4 byte crc, leaving decoder at the start of the next
    // chunk meta header.
    dec.seek(dataLen + 4, std::ios_base::cur);

    return {{seriesRef, std::move(ref)}};
}