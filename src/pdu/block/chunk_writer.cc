#include "chunk_writer.h"

#include <limits>

#include <boost/config.hpp>

ChunkWriter::ChunkWriter(std::ostream& out) : out(out), enc(out), bits(enc) {
    // TODO seek forward by 2 bytes for sample count
    sampleCountPosition = out.tellp();
    enc.write_int(uint16_t(0));
}

ChunkWriter::~ChunkWriter() {
    close();
}

void ChunkWriter::close() {
    if (closed()) {
        return;
    }
    // flush any remaining data out of the bitwise encoder
    bits.close();

    // seek backwards, write out sample count
    out.seekp(sampleCountPosition);
    enc.write_int(sampleCount);

    open = false;
}

void ChunkWriter::append(const Sample& s) {
    if (BOOST_UNLIKELY(closed())) {
        throw std::logic_error(
                "ChunkWriter::append cannot write more samples to a closed "
                "chunk");
    }
    if (BOOST_UNLIKELY(full())) {
        throw std::length_error(
                "ChunkWriter::append cannot write more samples to full chunk "
                "(max size " +
                std::to_string(std::numeric_limits<uint16_t>::max()) + ")");
    }

    if (BOOST_UNLIKELY(sampleCount == 0)) {
        // TODO write full sample
        enc.write_varint(s.timestamp);
        enc.write_int(reinterpret_cast<const uint64_t&>(s.value));
    } else if (BOOST_UNLIKELY(sampleCount == 1)) {
        if (BOOST_UNLIKELY(s.timestamp < prev.timestamp)) {
            throw std::logic_error(
                    "ChunkWriter::append cannot write samples with "
                    "non-monotonic timestamps prev: " +
                    std::to_string(prev.timestamp) +
                    " new:" + std::to_string(s.timestamp));
        }
        prev.tsDelta = s.timestamp - prev.timestamp;
        // last direct usage of encoder - after this everything goes
        // through the BitEncoder. BitEncoder buffers one byte, would
        // be wrong to mix and match.
        enc.write_varuint(prev.tsDelta);
        writeValue(s.value);
    } else {
        writeTSDod(s.timestamp);
        writeValue(s.value);
    }
    prev.timestamp = s.timestamp;
    prev.value = s.value;
    sampleCount++;
}

bool fitsInBits(int64_t dod, uint8_t nbits) {
    // see chunk_view.cc minBits for description of adjusted two's complement
    // used in prometheus - tl;dr 0b10...0 encodes the most positive value
    // rather than the most negative value.
    // see
    // https://github.com/prometheus/prometheus/blob/release-2.26/tsdb/chunkenc/xor.go#L203
    // for Prometheus version.
    return -int64_t((uint64_t(1) << (nbits - 1)) - 1) <= dod &&
           dod <= int64_t(uint64_t(1) << (nbits - 1));
}

void ChunkWriter::writeTSDod(int64_t timestamp) {
    int64_t tsDelta = timestamp - prev.timestamp;
    int64_t tsDod = tsDelta - prev.tsDelta;

    if (tsDod == 0) {
        bits.writeBit(0);
    } else if (fitsInBits(tsDod, 14)) {
        bits.writeBits(0b10, 2);
        bits.writeBits(uint64_t(tsDod), 14);
    } else if (fitsInBits(tsDod, 17)) {
        bits.writeBits(0b110, 3);
        bits.writeBits(uint64_t(tsDod), 17);
    } else if (fitsInBits(tsDod, 20)) {
        bits.writeBits(0b1110, 4);
        bits.writeBits(uint64_t(tsDod), 20);
    } else {
        bits.writeBits(0b1111, 4);
        bits.writeBits(uint64_t(tsDod), 64);
    }

    prev.tsDelta = tsDelta;
}

void ChunkWriter::writeValue(double val) {
    // translated from
    // https://github.com/prometheus/prometheus/blob/7309c20e7e5774e7838f183ec97c65baa4362edc/tsdb/chunkenc/xor.go#L220-L253
    // xor delta
    uint64_t vDelta = reinterpret_cast<uint64_t&>(val) ^
                      reinterpret_cast<uint64_t&>(prev.value);

    if (!vDelta) {
        // value is identical
        bits.writeBit(0);
        return;
    }
    bits.writeBit(1);
    auto leadingZeros = uint8_t(__builtin_clzll(vDelta));
    auto trailingZeros = uint8_t(__builtin_ctzll(vDelta));

    // 5 bits can encode max 0b11111, 31
    // If there were more leading zeroes, they'll just be included
    // in the delta value written to the chunk.
    if (leadingZeros >= 32) {
        leadingZeros = 31;
    }

    // if we have written a vdelta before, and we have at least as many
    // leading and trailing zeroes, there's no need to encode leading
    // and sig bits for this sample, format can "reuse" the ones from the prev
    // sample.
    if (prev.leading != std::numeric_limits<uint8_t>::max() &&
        leadingZeros >= prev.leading && trailingZeros >= prev.trailing) {
        bits.writeBit(0);
        bits.writeBits(vDelta >> prev.trailing,
                       64 - prev.leading - prev.trailing);
    } else {
        prev.leading = leadingZeros;
        prev.trailing = trailingZeros;

        bits.writeBit(1);
        // write the number of leading zeroes, encoded in 5 bits
        // (hence the max of 31)
        bits.writeBits(leadingZeros, 5);

        // encode the number of significant bits (bits which are not in the
        // leading or trailing zeroes)
        // Note that 64 would not be encodable in 6 bits.
        // instead, as 0 sig bits will never need to be encoded (would hit
        // !vDelta case above) reuse 0 to indicate 64 sig bits, which the
        // reading side will map back (see above link for Prometheus code
        // for this).
        // 64 & 0b111111 = 0;
        uint64_t sigBits = 64 - leadingZeros - trailingZeros;
        bits.writeBits(sigBits, 6);

        // finally, write the actual bits which have changed:
        bits.writeBits(vDelta >> trailingZeros, sigBits);
    }
}

bool ChunkWriter::empty() const {
    return sampleCount == 0;
}

bool ChunkWriter::full() const {
    return sampleCount == std::numeric_limits<uint16_t>::max();
}

ChunkWriter& operator<<(ChunkWriter& writer, const Sample& s) {
    writer.append(s);
    return writer;
}