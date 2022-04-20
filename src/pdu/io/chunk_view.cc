#include "chunk_view.h"

#include "chunk_reference.h"
#include "index.h"

#include <cmath>

bool operator==(const Sample& a, const Sample& b) {
    return a.timestamp == b.timestamp && a.value == b.value;
}

bool operator!=(const Sample& a, const Sample& b) {
    return !(a == b);
}

SampleIterator::SampleIterator(Decoder& dec, size_t sampleCount, bool rawChunk)
    : sampleCount(sampleCount), dec(&dec), bits(dec), rawChunk(rawChunk) {
    advance();
}

uint8_t minBits(ssize_t value) {
    if (value == 0) {
        return 1;
    }
    /* iterative equivalent:
     *  for (int nbits = 2; nbits < 64; ++nbits) {
     *    if (-((1 << (nbits - 1)) - 1) <= value
     *                                  && value <= (1 << (nbits - 1))) {
     *       return nbits;
     *    }
     *  }
     */

    auto v = std::abs((long long)value);

    /*
     * example:
     * minBits(4) -> 3
     * unlike typical 2's complement, prometheus _can_ encode 4 with 3 bits
     * 0b000 = 0
     * 0b001 = 1
     * 0b010 = 2
     * 0b011 = 3
     * 0b100 = 4, ceil(log2(0b100,     2)) + 1 = 2 + 1 = 3
     * 0b101 = -3 ceil(log2(0b011 + 1, 2)) + 1 = 2 + 1 = 3
     * 0b110 = -2
     * 0b111 = -1
     *
     * normally 0b100 would encode -4, but prometheus specifically uses this
     * as the highest positive value, 4.
     * when reading:
     * https://github.com/prometheus/prometheus/blob/release-2.26/tsdb/chunkenc/xor.go#L375
     * when determining if a value will fit in nbits:
     * https://github.com/prometheus/prometheus/blob/release-2.26/tsdb/chunkenc/xor.go#L203
     */
    auto bits =
            uint8_t(std::ceil(value > 0 ? std::log2(v) : std::log2(v + 1)) + 1);

    // A one-bit size category would only encode the value 1; 0 is always
    // encoded as 0b0 by Prometheus.
    // Assuming 1 is not frequent enough to warrant its own size category,
    // for all non-zero values the minimum number of bits to encode
    // is considered to be 2,
    // This leads to the smallest possible non-zero value range being
    // -1(0b11) <= x <= 2 (0b10)
    bits = std::max(bits, uint8_t(2));

    return bits;
}

void SampleIterator::increment() {
    ++currentIndex;
    if (is_end()) {
        return;
    }
    if (rawChunk) {
        s.timestamp = *reinterpret_cast<const int64_t*>(
                dec->read_view(sizeof(int64_t)).data());
        s.value = *reinterpret_cast<const double*>(
                dec->read_view(sizeof(double)).data());
        return;
    }
    if (currentIndex == 0) {
        {
            auto bc = bits.counter(s.meta.timestampBitWidth);
            prev.ts = s.timestamp = dec->read_varint();
        }
        auto val = dec->read_int<uint64_t>();
        prev.value = s.value = reinterpret_cast<double&>(val);

        s.meta.valueBitWidth = 64;

    } else if (currentIndex == 1) {
        auto startIdx = bits.tell();
        {
            auto bc = bits.counter(s.meta.timestampBitWidth);
            prev.tsDelta = dec->read_varuint();
        }
        prev.ts = s.timestamp = prev.ts + prev.tsDelta;

        {
            auto bc = bits.counter(s.meta.valueBitWidth);
            s.value = readValue();
        }
    } else {
        int64_t dod = 0;
        {
            auto bc = bits.counter(s.meta.timestampBitWidth);
            std::tie(s.timestamp, dod) = readTS();
        }
        s.meta.minTimestampBitWidth = minBits(dod);
        {
            auto bc = bits.counter(s.meta.valueBitWidth);
            s.value = readValue();
        }
    }
}

std::pair<int64_t, int64_t> SampleIterator::readTS() {
    auto dod = readTSDod();
    prev.tsDelta += dod;
    prev.ts += prev.tsDelta;
    return {prev.ts, dod};
}

int64_t SampleIterator::readTSDod() {
    uint8_t tsPrefix = 0;
    for (int i = 0; i < 4; ++i) {
        tsPrefix <<= 1;
        if (!bits.readBit()) {
            break;
        }
        tsPrefix |= 1;
    }

    // determine how many bits the ts is encoded in from the
    // prefix. These value ranges match those set in Prometheus' xor.go
    uint8_t tsBitCount = 0;
    switch (tsPrefix) {
    case 0x00: // 0
        // dod is zero
        return 0;
    case 0x02: // 10
        tsBitCount = 14;
        break;
    case 0x06: // 110
        tsBitCount = 17;
        break;
    case 0x0e: // 1110
        tsBitCount = 20;
        break;
    case 0x0f: // 1111
        tsBitCount = 64;
        break;
    default:
        throw std::logic_error("Invalid tsPrefix: " + std::to_string(tsPrefix));
    }

    size_t tsBits = bits.readBits(tsBitCount);

    // handle negative valuer
    if (tsBits > (1 << (tsBitCount - 1))) {
        return tsBits - (1 << tsBitCount);
    }
    return tsBits;
}

double SampleIterator::readValue() {
    if (!bits.readBit()) {
        // delta is zero
        return prev.value;
    }

    if (!bits.readBit()) {
        // reuse previous leading/trailing, saves 10 bits
    } else {
        // read leading/trailing
        prev.leading = bits.readBits(5);
        uint8_t sigBits = bits.readBits(6);
        if (sigBits == 0) {
            // 64 would overflow, and 0 is otherwise unused, so 0 is used
            // instead to encode 64.
            sigBits = 64;
        }
        prev.trailing = 64 - prev.leading - sigBits;
    }

    uint8_t sigBits = 64 - prev.leading - prev.trailing;

    if (!sigBits) {
        throw std::logic_error("Chunkfile read sigBits==0, this is not valid");
    }

    size_t newValueBits = bits.readBits(sigBits);
    newValueBits <<= prev.trailing;
    newValueBits ^= reinterpret_cast<uint64_t&>(prev.value);
    double newValue = reinterpret_cast<double&>(newValueBits);

    prev.value = newValue;

    return newValue;
}

ChunkView::ChunkView(ChunkFileCache& cfc, const ChunkReference& chunkRef)
    : baseOffset(chunkRef.getOffset()),
      res(cfc.get(chunkRef.getSegmentFileId())),
      dec(res->get()) {
    dec.seek(baseOffset);

    if (chunkRef.type == ChunkType::Raw) {
        rawChunk = true;
        dataOffset = 0;
        dataLen = dec.remaining();
        sampleCount = dataLen / (sizeof(int64_t) + sizeof(double));
        return;
    } else if (chunkRef.type == ChunkType::Head) {
        dec.read_int<uint64_t>(); // seriesRef
        dec.read_int<uint64_t>(); // minTime
        dec.read_int<uint64_t>(); // maxTime

        auto encoding = dec.read_int<uint8_t>();
        if (encoding != 1) {
            throw std::runtime_error("Head chunk file has unknown encoding: " +
                                     std::to_string(encoding));
        }

        dataLen = dec.read_varuint();
    } else {
        dataLen = dec.read_varuint();

        auto encoding = dec.read_int<uint8_t>();
        if (encoding != 1) {
            throw std::runtime_error("Chunk file has unknown encoding: " +
                                     std::to_string(encoding));
        }
    }
    sampleCount = dec.read_int<uint16_t>();
    dataOffset = dec.tell();
}
