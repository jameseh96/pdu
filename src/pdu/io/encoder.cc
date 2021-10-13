#include "encoder.h"

#include <iostream>
#include <string>

void Encoder::write_varuint(uint64_t value) {
    // max encoded size is 10 bytes
    for (int i = 0; i < 10; ++i) {
        auto b = uint8_t(value & 0x7f);
        value >>= 7;
        if (value) {
            b |= 0x80;
            write(reinterpret_cast<char*>(&b), sizeof(b));
        } else {
            write(reinterpret_cast<char*>(&b), sizeof(b));
            break;
        }
    }
}
void Encoder::write_varint(int64_t value) {
    auto v = uint64_t(value) << 1;
    if (value < 0) {
        v = ~v;
    }
    write_varuint(v);
}

void Encoder::write(std::string_view value) {
    output.write(value.data(), value.size());
}