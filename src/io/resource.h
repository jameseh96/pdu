#pragma once

#include "decoder.h"

namespace boost::filesystem {
class path;
}

// Abstract type for a resource able to return a Decoder on demand.
// Subclasses handle e.g., a mapped file.
// An Index holds a provided Resource for it's lifetime.
struct Resource {
    Decoder operator*() const {
        return get();
    }
    virtual Decoder get() const = 0;

    virtual const std::string& getDirectory() const = 0;

    virtual ~Resource();
};