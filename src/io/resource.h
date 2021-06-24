#pragma once

#include "decoder.h"

// Abstract type for a resource able to return a Decoder on demand.
// Subclasses handle e.g., a mapped file.
// An Index holds a provided Resource for it's lifetime.
struct Resource {
    Decoder operator*() const {
        return get();
    }
    virtual Decoder get() const = 0;

    virtual ~Resource();
};