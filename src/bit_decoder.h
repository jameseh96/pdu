#pragma once

#include "decoder.h"

class BitDecoder {
public:
    BitDecoder(Decoder& dec);

    uint64_t readBits(size_t count);

    bool readBit();

protected:
    static uint8_t getMask(size_t bitCount);

    uint8_t getBitsFromBuffer(size_t bitCount);

    Decoder& dec;
    uint8_t buffer;
    uint8_t remainingBits = 0;
};