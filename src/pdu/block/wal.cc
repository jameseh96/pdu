#include "wal.h"

#include "index.h"
#include "mapped_file.h"
#include "resource.h"

#include <boost/filesystem.hpp>

#include <snappy.h>
#include <numeric>

void WalLoader::load(const boost::filesystem::path& dataDir) {
    auto walDir = dataDir / "wal";
    // no need for a resource cache, WAL segments are read once.

    std::vector<std::string> walSegments;
    std::vector<std::string> checkpoints;

    namespace fs = boost::filesystem;
    for (const auto& walSegment : fs::directory_iterator(walDir)) {
        auto fname = walSegment.path().string();
        if (fname.find("checkpoint") != std::string::npos) {
            checkpoints.push_back(fname);
        } else {
            walSegments.push_back(fname);
        }
    }

    std::sort(walSegments.begin(), walSegments.end());
    std::sort(checkpoints.begin(), checkpoints.end());

    if (!checkpoints.empty()) {
        const auto& latestCkpt = checkpoints.back();
        auto ckptIdxStr = latestCkpt.substr(latestCkpt.find('.') + 1);
        auto ckptIdx = std::stoul(ckptIdxStr);

        // ignore any segments from before the checkpoint, just in case
        // any are lingering.
        walSegments.erase(
                std::remove_if(
                        walSegments.begin(),
                        walSegments.end(),
                        [ckptIdx](const auto& s) {
                            return std::stoul(fs::path(s).filename().string()) <
                                   ckptIdx;
                        }),
                walSegments.end());

        std::vector<std::string> ckptSegments;
        // load any segments from the checkpoint before the rest of the WAL
        for (const auto& walSegment : fs::directory_iterator(latestCkpt)) {
            ckptSegments.push_back(walSegment.path().string());
        }

        std::sort(ckptSegments.begin(), ckptSegments.end());

        // we need to visit the segments in the checkpoint first, so
        // just append the non-checkpoint segments to the end of this
        // vector and swap (saves inserting at the start and shuffling
        // all the elements along)
        ckptSegments.insert(
                ckptSegments.end(), walSegments.begin(), walSegments.end());

        std::swap(walSegments, ckptSegments);
    }

    for (int i = 0; i < walSegments.size(); ++i) {
        loadFile(walSegments[i], i == walSegments.size() - 1);
    }
}

void WalLoader::loadFile(const boost::filesystem::path& file, bool isLast) {
    auto resource = map_file(file);
    if (resource->empty()) {
        return;
    }
    auto dec = resource->getDecoder();
    while (!dec.empty()) {
        loadFragment(dec, isLast);
    }
}

void WalLoader::loadFragment(Decoder& dec, bool isLastFile) {
    std::string_view record;

    while (!dec.empty()) {
        auto type = dec.read_int<uint8_t>();

        if (type == PageEmpty) {
            // rest of page is empty, consume to next 32KB boundary
            auto pos = dec.tell();
            if (pos & 0x7fff) {
                pos &= ~0x7fff;
                pos += 0x8000;
            }
            if (dec.remaining() < (pos - dec.tell())) {
                if (isLastFile) {
                    // partial empty record, but that is okay for the end of the
                    // last file.
                    pos = dec.tell() + dec.remaining();
                } else {
                    throw std::logic_error(
                            "WAL: too few bytes left to read to page boundary");
                }
            }
            dec.seek(pos);
            clear();
            return;
        }

        if (dec.remaining() < 6) {
            if (isLastFile) {
                // this is the last block of the wal and may be incomplete
                // skip the decoder to the end and treat this block as done
                dec.read_view(dec.remaining());
                return;
            } else {
                throw std::logic_error("WAL: too few bytes for fragment meta");
            }
        }

        auto len = dec.read_int<uint16_t>();
        auto crc = dec.read_int<uint32_t>();

        if (dec.remaining() < len) {
            if (isLastFile) {
                // partial record, but that is okay for the end of the last
                // file. discard.
                dec.read_view(dec.remaining());
                return;
            } else {
                throw std::logic_error("WAL: too few bytes for fragment body");
            }
        }

        // 0b0000 empty to page boundary
        // 0b0001 full record
        // 0b0010 start of record fragment
        // 0b0011 middle of record fragment
        // 0b0100 end of record fragment
        // 0b1000 compressed

        if (type & Compressed) {
            needsDecompressing = true;
            // zero the compressed bit
            type &= ~Compressed;
        }

        if (type == RecordFull) {
            if (!rawBuffer.empty()) {
                throw std::logic_error(
                        "WAL: Complete fragment seen in middle of partial "
                        "fragments");
            }
            // is a complete record, process it.
            record = dec.read_view(len);
            break;
        }

        if (type == RecordStart) {
            if (inPartialFragment) {
                throw std::logic_error(
                        "WAL: Start fragment seen in middle of partial "
                        "fragments");
            }
            // is a partial record, read it into buffer
            auto view = dec.read_view(len);
            rawBuffer.insert(rawBuffer.end(), view.begin(), view.end());
            inPartialFragment = true;
            continue;
        }

        if (type == RecordMid) {
            if (!inPartialFragment) {
                throw std::logic_error(
                        "WAL: middle fragment seen before start");
            }
            // is the middle of a partial record, read it into buffer
            auto view = dec.read_view(len);
            rawBuffer.insert(rawBuffer.end(), view.begin(), view.end());
            continue;
        }

        if (type == RecordEnd) {
            if (!inPartialFragment) {
                throw std::logic_error("WAL: end fragment seen before start");
            }
            // is the end a partial record, read it into buffer and process it.
            auto view = dec.read_view(len);
            rawBuffer.insert(rawBuffer.end(), view.begin(), view.end());
            record = std::string_view(
                    reinterpret_cast<const char*>(rawBuffer.data()),
                    rawBuffer.size());
            inPartialFragment = false;
            break;
        }
        throw std::logic_error("WAL: unknown fragment type: " +
                               std::to_string(int(type)));
    }

    if (inPartialFragment || (record.empty() && !rawBuffer.empty())) {
        throw std::logic_error("WAL: incomplete record found");
    }

    if (record.empty()) {
        throw std::logic_error("WAL: empty record found");
    }

    if (needsDecompressing) {
        // decompress, reset record to point to decompressed data
        size_t length;
        if (!snappy::GetUncompressedLength(
                    record.data(), record.size(), &length)) {
            throw std::runtime_error(
                    "WAL: snappy decompression failed to get length");
        }
        decompressedBuffer.resize(length);

        if (!snappy::RawUncompress(record.data(),
                                   record.size(),
                                   (char*)decompressedBuffer.data())) {
            throw std::runtime_error(
                    "WAL: snappy decompression failed to decompress");
        }
        record = std::string_view(
                reinterpret_cast<const char*>(decompressedBuffer.data()),
                decompressedBuffer.size());
    }

    loadRecord(Decoder(record));

    clear();
}

void WalLoader::loadRecord(Decoder dec) {
    auto type = dec.read_int<uint8_t>();
    switch (type) {
    case 1:
        // Series definitions
        loadSeries(dec);
    case 2:
        // Samples
        loadSamples(dec);
    case 3:
        // Tombstone, ignore.
        break;
    default:
        throw std::invalid_argument(
                "WAL: Record contains unknown record type: " +
                std::to_string(int(type)));
    }
}

void WalLoader::loadSeries(Decoder& dec) {
    while (!dec.empty()) {
        auto seriesId = dec.read_int<uint64_t>();
        auto& series = seriesMap[seriesId];

        auto labelCount = dec.read_varuint();

        for (int i = 0; i < labelCount; ++i) {
            auto len = dec.read_varuint();
            auto key = addSymbol(dec.read_view(len));
            len = dec.read_varuint();
            auto value = addSymbol(dec.read_view(len));
            series.labels.emplace(key, value);
        }
    }
}

InMemWalChunk::InMemWalChunk() {
    // reserving space for at least 100 samples for this time series
    // might be an overestimate, but probably better than
    // reallocating too often.
    data.reserve(100 * (sizeof(int64_t) + sizeof(double)));
}

void InMemWalChunk::setMinTime(int64_t ts) {
    minTime = ts;
}
void InMemWalChunk::addSample(int64_t ts, double value) {
    if (ts < minTime) {
        return;
    }
    maxTime = ts > maxTime ? ts : maxTime;
    auto pos = data.size();
    data.resize(pos + sizeof(int64_t) + sizeof(double));

    std::memcpy(&data[pos], &ts, sizeof(ts));
    std::memcpy(&data[pos + sizeof(ts)], &value, sizeof(value));
}

std::pair<std::shared_ptr<Resource>, ChunkReference>
InMemWalChunk::makeResource() const {
    ChunkReference ref;
    ref.type = ChunkType::Raw;
    ref.minTime = minTime;
    ref.maxTime = maxTime;

    auto resource = std::make_shared<MemResource>(
            std::string_view((char*)data.data(), data.size()));

    return {std::move(resource), std::move(ref)};
}

void WalLoader::loadSamples(Decoder& dec) {
    if (dec.empty()) {
        return;
    }
    auto baseRef = dec.read_int<uint64_t>();
    auto baseTs = int64_t(dec.read_int<uint64_t>());

    while (!dec.empty()) {
        // read deltas
        auto dRef = dec.read_varint();
        auto dTs = dec.read_varint();

        auto valRaw = dec.read_int<uint64_t>();
        double value = reinterpret_cast<double&>(valRaw);

        auto ref = uint64_t(baseRef + dRef);
        auto ts = int64_t(baseTs + dTs);

        auto seriesItr = seriesMap.find(ref);
        if (seriesItr == seriesMap.end()) {
            continue;
        }

        auto& headChunks = seriesItr->second.chunks;

        // if this is the first sample from the WAL for a given TS, set the
        // min time so duplicate samples can be discarded (Head chunks and WAL
        // may overlap)
        if (walChunks.find(ref) == walChunks.end() && !headChunks.empty()) {
            walChunks[ref].setMinTime(headChunks.back().maxTime + 1);
        }

        walChunks[ref].addSample(ts, value);
    }
}

std::string_view WalLoader::addSymbol(std::string_view sym) {
    auto symbolItr = symbols.find(sym);
    if (symbolItr == symbols.end()) {
        auto res = symbols.insert(std::string(sym));
        symbolItr = res.first;
    }
    return *symbolItr;
}