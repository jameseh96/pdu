#include "posting_offset_iterator.h"

void PostingOffset::load(Decoder& dec) {
    auto constant = dec.read_int<uint8_t>();
    if (constant != 2) {
        throw std::runtime_error("Unexpected constant in PostingOffset : " +
                                 std::to_string(constant));
    }
    auto nameLen = dec.read_varuint();
    labelKey = dec.read_view(nameLen);
    auto valLen = dec.read_varuint();
    labelValue = dec.read_view(valLen);
    offset = dec.read_varuint();
}

PostingOffsetIterator::PostingOffsetIterator(Decoder dec, size_t count)
    : dec(dec), count(count) {
    advance();
}

void PostingOffsetIterator::increment() {
    ++currentIndex;
    if (!is_end()) {
        postingOffset.load(dec);
    }
}