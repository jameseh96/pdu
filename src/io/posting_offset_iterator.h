#pragma once

#include "decoder.h"
#include "util/generator_iterator.h"

struct PostingOffset {
    std::string_view labelKey;
    std::string_view labelValue;
    size_t offset;

    void load(Decoder& dec);
};

class PostingOffsetIterator
    : public generator_iterator<PostingOffsetIterator, PostingOffset> {
public:
    PostingOffsetIterator(Decoder dec, size_t count);

    bool next(PostingOffset& postingOffset);

private:
    Decoder dec;
    size_t count;
    ssize_t currentIndex = 0;
};