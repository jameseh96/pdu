#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iosfwd>
#include <string>
#include <string_view>
#include <utility>

#include "pdu/util/host.h"

class Encoder {
public:
    Encoder(std::ostream& os) : output(os) {
    }
    void write_varuint(uint64_t value);
    void write_varint(int64_t value);

    template <class T>
    void write_int(T value) {
        auto netval = from_host(value);
        write(reinterpret_cast<char*>(&netval), sizeof(netval));
    }

    void write(char* str, size_t count) {
        write(std::string_view(str, count));
    }

    void write(std::string_view value);

private:
    std::ostream& output;
};
