#pragma once

#include "pdu/block/sample.h"
#include "pdu/encode/bit_encoder.h"
#include "pdu/encode/encoder.h"

#include <iostream>
#include <limits>

class ChunkWriter {
public:
    ChunkWriter(std::ostream& out);

    ~ChunkWriter();

    void close();

    void append(const Sample& s);

    bool empty() const;

    bool full() const;

    bool closed() const {
        return !open;
    }

private:
    void writeTSDod(int64_t timestamp);
    void writeValue(double val);

    std::ostream& out;
    Encoder enc;
    BitEncoder bits;
    uint16_t sampleCount = 0;

    struct {
        int64_t timestamp = 0;
        int64_t tsDelta = 0;
        double value = 0;
        uint8_t leading = std::numeric_limits<uint8_t>::max();
        uint8_t trailing = 0;
    } prev;

    std::ostream::pos_type sampleCountPosition;

    bool open = true;
};

ChunkWriter& operator<<(ChunkWriter& writer, const Sample& s);
