#pragma once

#include <cstddef>
#include <cstdint>

class Encoder;

class BitEncoder {
public:
    BitEncoder(Encoder& enc);
    ~BitEncoder();

    /**
     * write the low @p count bits from the value to the output
     */
    void writeBits(uint64_t value, size_t count);

    void writeBit(bool val);

    // flush out the bit-wise buffer. No further bits should be written.
    void close();

    bool closed() const;

protected:
    static uint8_t getMask(size_t bitCount);

    Encoder& enc;
    struct State {
        uint8_t buffer = 0;
        uint8_t remainingBits = 8;
    } state;

    bool open = true;
};