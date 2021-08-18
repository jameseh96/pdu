#pragma once

#include "decoder.h"

class BitDecoder;

/**
 * RAII helper for recording how many bits have been read between the time of
 * construction and destruction.
 */
class BitCounter {
public:
    BitCounter(const BitDecoder& bits, uint16_t& dest);
    ~BitCounter();

private:
    const BitDecoder& bits;
    size_t initial = 0;
    uint16_t& dest;
};

class BitDecoder {
public:
    BitDecoder() = default;
    BitDecoder(Decoder& dec);

    uint64_t readBits(size_t count);

    bool readBit();

    size_t tell() const;

    BitCounter counter(uint16_t& dest) const;

protected:
    static uint8_t getMask(size_t bitCount);

    uint8_t getBitsFromBuffer(size_t bitCount);

    Decoder* dec;
    uint8_t buffer;
    uint8_t remainingBits = 0;
};