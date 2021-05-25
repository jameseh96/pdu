#pragma once

#include <fstream>
#include <iostream>
#include <string>
#include <utility>

template <class T>
T to_host(T);

template <>
uint8_t to_host(uint8_t v);

template <>
uint16_t to_host(uint16_t v);

template <>
uint32_t to_host(uint32_t v);

template <>
uint64_t to_host(uint64_t v);

class Decoder {
public:
    Decoder(std::istream& stream) : stream(stream) {
    }

    uint64_t read_varuint();

    uint64_t read_varint();

    template <class T>
    T read_int() {
        char bytes[sizeof(T)];
        stream.read(bytes, sizeof(T));
        return T(to_host(*reinterpret_cast<T*>(bytes)));
    }

    template <class T>
    void read_int_to(T& v) {
        char bytes[sizeof(T)];
        stream.read(bytes, sizeof(T));
        v = T(to_host(*reinterpret_cast<T*>(bytes)));
    }

    void consume_null();

    size_t consume_to_alignment(size_t alignment);

    Decoder& read(char* dest, size_t count);

    std::string read(size_t count);

    auto tellg() {
        return stream.tellg();
    }

    template <class... Args>
    Decoder& seekg(Args&&... args) {
        stream.seekg(std::forward<Args>(args)...);
        return *this;
    }

private:
    std::istream& stream;
};