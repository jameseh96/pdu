#include "bit_decoder.h"

BitCounter::BitCounter(const BitDecoder& bits, uint16_t& dest)
    : bits(bits), initial(bits.tell()), dest(dest) {
}
BitCounter::~BitCounter() {
    dest = uint16_t(bits.tell() - initial);
}

BitDecoder::BitDecoder(Decoder& dec, BitDecoder::State& state)
    : dec(dec), state(state) {
}

uint64_t BitDecoder::readBits(size_t count) {
    if (count > 64) {
        throw std::logic_error(
                "Only support reading 64 bits at a time, tried to read: " +
                std::to_string(count));
    }

    size_t result = 0;
    while (count > 0) {
        if (state.remainingBits == 0) {
            dec.read_int_to(state.buffer);
            state.remainingBits = 8;
        }
        auto bitsToRead = std::min(count, size_t(state.remainingBits));

        result <<= bitsToRead;
        result |= getBitsFromBuffer(bitsToRead);
        count -= bitsToRead;
        state.remainingBits -= bitsToRead;
    };

    return result;
}

bool BitDecoder::readBit() {
    return bool(readBits(1) & 1);
}

size_t BitDecoder::tell() const {
    return size_t(dec.tell()) * 8 - state.remainingBits;
}

BitCounter BitDecoder::counter(uint16_t& dest) const {
    return {*this, dest};
}

uint8_t BitDecoder::getMask(size_t bitCount) {
    return (uint8_t(1) << bitCount) - 1;
}

uint8_t BitDecoder::getBitsFromBuffer(size_t bitCount) {
    auto mask = getMask(bitCount);
    mask <<= (state.remainingBits - bitCount);
    auto value = state.buffer & mask;
    value >>= (state.remainingBits - bitCount);
    return value;
}