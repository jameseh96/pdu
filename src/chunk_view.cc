#include "chunk_view.h"

#include "index.h"

SampleIterator::SampleIterator(Decoder& dec, size_t sampleCount)
    : sampleCount(sampleCount), dec(dec), bits(dec) {
    advance();
}

bool SampleIterator::next(Sample& s) {
    if (currentIndex == sampleCount) {
        return false;
    }
    if (currentIndex == 0) {
        {
            auto bc = bits.counter(s.meta.timestampBitWidth);
            prev.ts = s.timestamp = dec.read_varint();
        }
        auto val = dec.read_int<uint64_t>();
        prev.value = s.value = reinterpret_cast<double&>(val);

        s.meta.valueBitWidth = 64;

    } else if (currentIndex == 1) {
        auto startIdx = bits.tell();
        {
            auto bc = bits.counter(s.meta.timestampBitWidth);
            prev.tsDelta = dec.read_varuint();
        }
        prev.ts = s.timestamp = prev.ts + prev.tsDelta;

        {
            auto bc = bits.counter(s.meta.valueBitWidth);
            s.value = readValue();
        }
    } else {
        {
            auto bc = bits.counter(s.meta.timestampBitWidth);
            s.timestamp = readTS();
        }
        {
            auto bc = bits.counter(s.meta.valueBitWidth);
            s.value = readValue();
        }
    }

    ++currentIndex;
    return true;
}

int64_t SampleIterator::readTS() {
    auto dod = readTSDod();
    prev.tsDelta += dod;
    prev.ts += prev.tsDelta;
    return prev.ts;
}

int64_t SampleIterator::readTSDod() {
    uint8_t tsPrefix;
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
    size_t newValueBits = bits.readBits(sigBits);
    newValueBits <<= prev.trailing;
    newValueBits ^= reinterpret_cast<uint64_t&>(prev.value);
    double newValue = reinterpret_cast<double&>(newValueBits);

    prev.value = newValue;

    return newValue;
}

ChunkView::ChunkView(ChunkFileCache& cfc, const ChunkReference& chunkRef)
    : dec(cfc.get(chunkRef.getSegmentFileId())) {
    dec.seekg(chunkRef.getOffset());
    dataLen = dec.read_varuint();

    auto encoding = dec.read_int<uint8_t>();
    if (encoding != 1) {
        throw std::runtime_error("Chunk file has unknown encoding: " +
                                 std::to_string(encoding));
    }

    sampleCount = dec.read_int<uint16_t>();
    dataOffset = dec.tellg();
}
