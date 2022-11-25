#pragma once

#include "pdu/encode/decoder.h"

namespace boost::filesystem {
class path;
}

// Abstract type for a resource able to return a Decoder on demand.
// Subclasses handle e.g., a mapped file.
// An Index holds a provided Resource for it's lifetime.
struct Resource {
    Decoder operator*() const {
        return getDecoder();
    }
    virtual Decoder getDecoder() const = 0;

    virtual std::string_view getView() const = 0;

    virtual const std::string& getDirectory() const = 0;

    virtual bool empty() const = 0;

    virtual ~Resource();
};

struct MemResource : public Resource {
    MemResource(std::string_view data) : data(data) {
    }

    Decoder getDecoder() const override {
        return {data};
    }

    std::string_view getView() const override {
        return {data};
    }

    const std::string& getDirectory() const override {
        throw std::runtime_error("MemResource::getDirectory() not implemented");
    }

    bool empty() const override {
        return data.empty();
    }

private:
    std::string_view data;
};

struct OwningMemResource : public Resource {
    OwningMemResource(std::string data) : data(std::move(data)) {
    }

    Decoder getDecoder() const override {
        return {data};
    }

    std::string_view getView() const override {
        return {data};
    }

    const std::string& getDirectory() const override {
        throw std::runtime_error(
                "OwningMemResource::getDirectory() not implemented");
    }

    bool empty() const override {
        return data.empty();
    }

private:
    std::string data;
};