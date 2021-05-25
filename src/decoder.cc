#include "decoder.h"

#include <fstream>
#include <iostream>
#include <string>
#include <utility>

template <>
uint8_t to_host(uint8_t v) {
    return v;
}

#if (defined __APPLE__)
#include <arpa/inet.h>
template <>
uint16_t to_host(uint16_t v) {
    return ntohs(v);
}

template <>
uint32_t to_host(uint32_t v) {
    return ntohl(v);
}

template <>
uint64_t to_host(uint64_t v) {
    return ntohll(v);
}
#elif (defined __linux__)
#include <endian.h>

template <>
uint16_t to_host(uint16_t v) {
    return be16toh(v);
}

template <>
uint32_t to_host(uint32_t v) {
    return be32toh(v);
}

template <>
uint64_t to_host(uint64_t v) {
    return be64toh(v);
}
#endif

uint64_t Decoder::read_varuint() {
    uint8_t byte;

    stream.read(reinterpret_cast<char*>(&byte), 1);
    if (byte < 128) {
        return byte;
    }
    uint64_t value = byte & 0x7f;
    unsigned shift = 7;
    do {
        stream.read(reinterpret_cast<char*>(&byte), 1);
        value |= uint64_t(byte & 0x7f) << shift;
        shift += 7;
    } while (byte >= 128);
    return value;
}

uint64_t Decoder::read_varint() {
    auto raw = read_varuint();
    auto value = raw >> 1;
    if (raw & 1) {
        value = ~value;
    }
    return value;
}

void Decoder::consume_null() {
    while (stream.peek() == 0) {
        stream.get();
    }
}

size_t Decoder::consume_to_alignment(size_t alignment) {
    auto pos = stream.tellg();
    auto remainder = pos % alignment;
    if (!remainder) {
        return pos;
    }
    stream.seekg(16 - remainder, std::ios_base::cur);
    return stream.tellg();
}

Decoder& Decoder::read(char* dest, size_t count) {
    stream.read(dest, count);
    return *this;
}

std::string Decoder::read(size_t count) {
    std::string value;
    value.resize(count);
    stream.read(value.data(), count);
    return value;
}
