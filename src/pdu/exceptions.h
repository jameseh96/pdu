#pragma once

#include <stdexcept>

namespace pdu {
// wrapper for a runtime_error, raised when a decoder runs out of bytes
// used to allow pypdu to distinguish EOF from other runtime errors.
class EOFError : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

class unknown_encoding_error : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

} // namespace pdu