#pragma once

#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
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
    Decoder() = default;
    Decoder(const char* data, size_t size)
        : Decoder(std::string_view(data, size)) {
    }
    Decoder(std::string_view view) : view(view), subview(view) {
    }

    Decoder substr(size_t pos = 0, size_t count = std::string_view::npos) const;

    uint64_t read_varuint();
    uint64_t read_varint();

    template <class T>
    T read_int() {
        char bytes[sizeof(T)];
        read(bytes, sizeof(T));
        return T(to_host(*reinterpret_cast<T*>(bytes)));
    }

    template <class T>
    void read_int_to(T& v) {
        char bytes[sizeof(T)];
        read(bytes, sizeof(T));
        v = T(to_host(*reinterpret_cast<T*>(bytes)));
    }

    void consume_null();

    size_t consume_to_alignment(size_t alignment);

    std::string read(size_t count);

    std::string_view read_view(size_t count);

    Decoder& seek(size_t offset);

    Decoder& seek(size_t offset, std::ios_base::seekdir seekdir);

    size_t tell() const;

    Decoder& read(char* dest, size_t count);

    char peek() const;

    size_t remaining() const;

    bool empty() const;

private:
    std::string_view view;
    std::string_view subview;
};
