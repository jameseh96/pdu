#include "decoder.h"

#include "../exceptions.h"

#include <fstream>
#include <iostream>
#include <string>
#include <utility>

#include <fmt/format.h>

template <class Derived>
uint64_t DecoderBase<Derived>::read_varuint() {
    uint8_t byte;

    impl().read(reinterpret_cast<char*>(&byte), 1);
    if (byte < 128) {
        return byte;
    }
    uint64_t value = byte & 0x7f;
    unsigned shift = 7;
    do {
        impl().read(reinterpret_cast<char*>(&byte), 1);
        value |= uint64_t(byte & 0x7f) << shift;
        shift += 7;
    } while (byte >= 128);
    return value;
}

template <class Derived>
uint64_t DecoderBase<Derived>::read_varint() {
    auto raw = read_varuint();
    auto value = raw >> 1;
    if (raw & 1) {
        value = ~value;
    }
    return value;
}

template <class Derived>
void DecoderBase<Derived>::consume_null() {
    while (impl().peek() == 0) {
        impl().seek(1, std::ios_base::cur);
    }
}

template <class Derived>
size_t DecoderBase<Derived>::consume_to_alignment(size_t alignment) {
    auto pos = impl().tell();
    auto remainder = pos % alignment;
    if (!remainder) {
        return pos;
    }
    impl().seek(16 - remainder, std::ios_base::cur);
    return impl().tell();
}

template <class Derived>
std::string DecoderBase<Derived>::read(size_t count) {
    std::string value;
    value.resize(count);
    impl().read(value.data(), count);
    return value;
}

template <class Derived>
Derived& DecoderBase<Derived>::seek(size_t offset) {
    impl().seek(offset, std::ios_base::beg);
    return impl();
}

template class DecoderBase<Decoder>;
template class DecoderBase<StreamDecoder>;

//////

Decoder Decoder::substr(size_t pos, size_t count) const {
    return {view.substr(pos, count)};
}

std::string_view Decoder::read_view(size_t count) {
    if (count > subview.size()) {
        throw pdu::EOFError(
                fmt::format("read_view: reading {} bytes, only {} left",
                            count,
                            subview.size()));
    }
    auto value = subview.substr(0, count);
    subview.remove_prefix(count);
    return value;
}

Decoder& Decoder::seek(size_t offset, std::ios_base::seekdir seekdir) {
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

size_t Decoder::tell() const {
    return subview.data() - view.data();
}

Decoder& Decoder::read(char* dest, size_t count) {
    if (count > subview.size()) {
        throw pdu::EOFError(fmt::format(
                "read: reading {} bytes, only {} left", count, subview.size()));
    }
    memcpy(dest, subview.data(), count);
    subview.remove_prefix(count);
    return *this;
}

char Decoder::peek() const {
    if (subview.empty()) {
        throw pdu::EOFError("peek: no bytes left");
    }
    return subview[0];
}

size_t Decoder::remaining() const {
    return subview.size();
}

bool Decoder::empty() const {
    return subview.empty();
}

//////

StreamDecoder::StreamDecoder(std::istream& stream) : stream(stream) {
}

StreamDecoder& StreamDecoder::seek(size_t offset,
                                   std::ios_base::seekdir seekdir) {
    stream.seekg(offset, seekdir);
    return *this;
}

size_t StreamDecoder::tell() const {
    return stream.tellg();
}

StreamDecoder& StreamDecoder::read(char* dest, size_t count) {
    stream.read(dest, count);
    return *this;
}

char StreamDecoder::peek() const {
    return stream.peek();
}