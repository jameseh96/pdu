#pragma once

#include "pdu/encode/decoder.h"
#include "pdu/util/iterator_facade.h"

struct PostingOffset {
    std::string_view labelKey;
    std::string_view labelValue;
    size_t offset;

    void load(Decoder& dec);
};

class PostingOffsetIterator
    : public iterator_facade<PostingOffsetIterator, PostingOffset> {
public:
    PostingOffsetIterator(Decoder dec, size_t count);

    void increment();

    const PostingOffset& dereference() const {
        return postingOffset;
    }

    bool is_end() const {
        return currentIndex == count;
    }

private:
    PostingOffset postingOffset;
    Decoder dec;
    size_t count;
    ssize_t currentIndex = -1;
};