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
    Decoder(const char* data, size_t size) : view(data, size), subview(view) {
    }

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

    Decoder& seek(size_t offset, std::ios_base::seekdir seekdir) {
        switch (seekdir) {
        case std::ios_base::cur:
            subview = subview.substr(offset);
            break;
        case std::ios_base::beg:
            subview = view.substr(offset);
            break;
        case std::ios_base::end:
            subview = view.substr(view.size() + offset);
            break;
        default:
            // Linux complains about unhandled _S_ios_seekdir_end
            // default case to silence it
            throw std::logic_error("Unknown seekdir");
        }

        return *this;
    }

    size_t tell() {
        return subview.data() - view.data();
    }

    Decoder& read(char* dest, size_t count) {
        if (count > subview.size()) {
            throw std::runtime_error("read: too few left");
        }
        memcpy(dest, subview.data(), count);
        subview.remove_prefix(count);
        return *this;
    }

    char peek() {
        if (subview.empty()) {
            throw std::runtime_error("peek: no bytes left");
        }
        return subview[0];
    }

private:
    std::string_view view;
    std::string_view subview;
};
