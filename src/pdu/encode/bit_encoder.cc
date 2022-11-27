#include "bit_encoder.h"

#include "pdu/encode.h"

#include <boost/config.hpp>

BitEncoder::BitEncoder(Encoder& enc) : enc(enc) {
}

BitEncoder::~BitEncoder() {
    if (!closed()) {
        close();
    }
}

void BitEncoder::writeBits(uint64_t value, size_t count) {
    if (BOOST_UNLIKELY(closed())) {
        throw std::logic_error(
                "BitEncoder::writeBits called on closed BitEncoder");
    }
    if (BOOST_UNLIKELY(count > 64)) {
        throw std::logic_error(
                "Only support writing 64 bits at a time, tried to write: " +
                std::to_string(count));
    }

    while (count > 0) {
        auto bitsToWrite = std::min(count, size_t(state.remainingBits));

        if (bitsToWrite == 8) {
            // buffer is empty and we're writing at least a whole byte,
            // skip faffing with the buffer:
            enc.write_int(uint8_t(value >> (count - 8)));
            count -= 8;
            continue;
        }

        state.buffer |=
                (getMask(bitsToWrite) & (value >> (count - bitsToWrite)))
                << (state.remainingBits - bitsToWrite);
        count -= bitsToWrite;
        state.remainingBits -= bitsToWrite;

        if (state.remainingBits == 0) {
            enc.write_int(state.buffer);
            state.buffer = 0;
            state.remainingBits = 8;
        }
    };
}

void BitEncoder::writeBit(bool val) {
    writeBits(uint64_t(val), 1);
}

uint8_t BitEncoder::getMask(size_t bitCount) {
    return (uint8_t(1) << bitCount) - 1;
}

void BitEncoder::close() {
    if (closed()) {
        return;
    }
    if (state.remainingBits != 8) {
        enc.write_int(state.buffer);
    }
    open = false;
}

bool BitEncoder::closed() const {
    return !open;
}