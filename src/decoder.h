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
    virtual ~Decoder();

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

    Decoder& seek(size_t offset);

    // interface to be implemented by subtypes
    virtual Decoder& seek(size_t offset, std::ios_base::seekdir seekdir) = 0;

    virtual size_t tell() = 0;

    virtual Decoder& read(char* dest, size_t count) = 0;

    virtual char peek() = 0;
};

class StreamDecoder : public Decoder {
public:
    StreamDecoder(std::istream& stream) : stream(stream) {
    }
    using Decoder::seek;

    Decoder& seek(size_t offset, std::ios_base::seekdir seekdir) override {
        stream.seekg(offset, seekdir);
        return *this;
    }

    size_t tell() override {
        return stream.tellg();
    }

    Decoder& read(char* dest, size_t count) override {
        stream.read(dest, count);
        return *this;
    }

    char peek() override {
        return stream.peek();
    }

private:
    std::istream& stream;
};