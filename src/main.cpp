#include "chunk_file_cache.h"
#include "chunk_view.h"
#include "decoder.h"
#include "display_units.h"
#include "file_map.h"
#include "index.h"
#include "index_iterator.h"

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>

enum class SortOrder { Default, Size };

std::istream& operator>>(std::istream& in, SortOrder& sort) {
    std::string token;
    in >> token;

    // modifies token
    boost::algorithm::to_lower(token);

    if (token == "default") {
        sort = SortOrder::Default;
    } else if (token == "size") {
        sort = SortOrder::Size;
    } else {
        in.setstate(std::ios_base::failbit);
    }
    return in;
}
// name, size, percentage
using Value = std::tuple<std::string_view, size_t>;
std::function<bool(Value, Value)> makeComparator(SortOrder order,
                                                 bool reverse = false) {
    if (order != SortOrder::Size) {
        throw std::logic_error("Unknown sort order");
    }
    std::function<bool(Value, Value)> comparator = [](const auto& a,
                                                      const auto& b) {
        return std::get<1>(a) < std::get<1>(b);
    };

    if (reverse) {
        comparator = [inner = std::move(comparator)](const auto& a,
                                                     const auto& b) {
            return inner(b, a);
        };
    }
    return comparator;
}

struct params_t {
    params_t(int argc, char* argv[]) {
        namespace po = boost::program_options;

        boost::program_options::options_description options("Allowed options");
        boost::program_options::positional_options_description pos_options;

        // clang-format off
        options.add_options()
            ("dir,d", po::value(&statsDir)->required(), "Prometheus stats directory")
            ("total,c", po::bool_switch(&total), "Print total")
            ("summary,s", po::bool_switch(&summary), "Print only summary")
            ("human,h", po::bool_switch(&human), "Use \"human-readable\" units")
            ("percent,p", po::bool_switch(&percent), "Display percentage of total usage")
            ("sort,S", po::value(&sort), "Sort output, valid values: \"default\", \"size\"")
            ("reverse,r", po::bool_switch(&reverse), "Reverse sort order")
            ("bitwidth,b", po::bool_switch(&showBitwidth), "Display timestamp/value encoding bit width distributions");

        pos_options.add("dir", 1);
        // clang-format on

        po::variables_map vm;
        try {
            auto parsed = po::command_line_parser(argc, argv)
                                  .options(options)
                                  .positional(pos_options)
                                  .run();
            po::store(parsed, vm);
            po::notify(vm);
            valid = true;
        } catch (const po::error& e) {
            fmt::print("{}\n", e.what());
            fmt::print(
                    "Usage:\n"
                    "    pdu <options> <dir>\n"
                    "e.g.,\n"
                    "    pdu -ch ./stats_data\n\n");
            fmt::print("{}\n", options);
            valid = false;
        }

        // printing the summary implies printing the total without
        // the rest of the breakdown.
        if (summary) {
            total = true;
        }
    }
    std::string statsDir = "";
    bool total = false;
    bool summary = false;
    bool human = false;
    bool percent = false;
    SortOrder sort = SortOrder::Default;
    bool reverse = false;
    bool showBitwidth = false;
    bool valid = false;
};

struct BitWidthHistogram {
    void record(uint16_t value) {
        ++values[value];
    }

    size_t totalSize() const {
        size_t total = 0;
        for (const auto& [size, count] : values) {
            total += size * count;
        }
        return total;
    }

    BitWidthHistogram& operator+=(const BitWidthHistogram& other) {
        for (const auto& [size, count] : other.values) {
            values[size] += count;
        }
        return *this;
    }

    std::map<uint16_t, uint64_t> values;
};

void printHistogram(const BitWidthHistogram& hist,
                    bool percent = false,
                    bool human = false) {
    size_t totalCount = 0;
    size_t totalSize = 0;

    for (const auto& [size, count] : hist.values) {
        totalCount += count;
        totalSize += size * count;
    }
    fmt::print("  total size: ");
    if (human) {
        auto [scaled, unit] = format::humanReadableBytes(totalSize);
        fmt::print("{:<7}", fmt::format("{}{}", scaled, unit));
    } else {
        fmt::print("{:<7}", totalSize);
    }
    fmt::print("\n");
    for (const auto& [bits, count] : hist.values) {
        fmt::print("    {:>2}b: {:>10}", bits, count);
        if (percent) {
            fmt::print(" {:>7.2f}% count, {:>7.2f}% size",
                       double(count * 100) / totalCount,
                       double(bits * count * 100) / totalSize);
        }

        fmt::print("\n");
    }
}

struct SampleWidthHistograms {
    BitWidthHistogram timestamps;
    BitWidthHistogram values;

    SampleWidthHistograms& operator+=(const SampleWidthHistograms& other) {
        timestamps += other.timestamps;
        values += other.values;
        return *this;
    }

    operator size_t() const {
        return timestamps.totalSize() + values.totalSize();
    }
};

void printKV(std::string_view key,
             size_t value,
             size_t total,
             const params_t& params) {
    // print the value
    if (params.human) {
        auto [scaled, unit] = format::humanReadableBytes(value);
        fmt::print("{:<7}", fmt::format("{}{}", scaled, unit));
    } else {
        fmt::print("{:<7}", value);
    }

    // maybe print a percentage of the total
    if (params.percent) {
        fmt::print(" {:>7}",
                   fmt::format("{:.2f}%", double(value * 100) / total));
    }

    // print name
    fmt::print(" {}\n", key);
}

void printKV(std::string_view key,
             const SampleWidthHistograms& hists,
             SampleWidthHistograms,
             const params_t& params) {
    // print name
    fmt::print("{}\n", key);

    fmt::print("  Timestamps\n");
    printHistogram(hists.timestamps, params.percent, params.human);

    fmt::print("  Values\n");
    printHistogram(hists.values, params.percent, params.human);
}

/**
 * Write a collection of key-value pairs to stdout in a configurable, du-esque
 * format.
 */
template <class T>
void display(const T& data, const params_t& params) {
    // sum up all values if total or percentage required
    typename T::mapped_type total{};
    if (params.total || params.percent) {
        for (const auto& series : data) {
            total += series.second;
        }
    }

    auto print = [total, &params](auto&& key, auto&& value) {
        printKV(key, value, total, params);
    };

    if (params.total) {
        print("total", total);
    }

    if (params.summary) {
        // stop here, don't print the rest of the data
        return;
    }

    if (params.sort == SortOrder::Default) {
        for (const auto& [key, value] : data) {
            print(key, value);
        }
    } else {
        std::vector<Value> values;
        for (const auto& [key, value] : data) {
            values.emplace_back(key, size_t(value));
        }

        auto comparator = makeComparator(params.sort, params.reverse);

        std::sort(values.begin(), values.end(), comparator);

        for (const auto& val : values) {
            auto key = std::get<0>(val);
            print(key, data.find(key)->second);
        }
    }
}

template <class Value, class Callable>
auto accumulate(const boost::filesystem::path& dirPath, Callable&& cb) {
    // std::less<> allows for hetrogenous lookup
    std::map<std::string, Value, std::less<>> result;

    // iterate over every chunk index in the provided directory
    for (const auto& [index, subdir] : IndexIterator(dirPath)) {
        // Once a chunk file reference is encountered in the index, the
        // appropriate chunk file will be mmapped and inserted into the cache
        // as they are likely to be used again.
        ChunkFileCache cache(subdir / "chunks");

        // iterate over each time series in the index
        for (const auto& tableEntry : index.series) {
            const auto& series = tableEntry.second;
            auto name = series.labels.at("__name__");

            auto itr = result.find(name);

            if (itr == result.end()) {
                itr = result.try_emplace(std::string(name), Value{}).first;
            }

            for (const auto& chunk : series.chunks) {
                // ChunkView parses the chunk start info, but will not read
                // all samples unless they are iterated over.
                ChunkView view(cache, chunk);
                cb(view, itr->second);
            }
        }
    }

    return result;
}

int main(int argc, char* argv[]) {
    params_t params(argc, argv);
    if (!params.valid) {
        return 1;
    }

    namespace fs = boost::filesystem;

    fs::path dirPath = params.statsDir;

    if (params.showBitwidth) {
        display(accumulate<SampleWidthHistograms>(
                        dirPath,
                        [](ChunkView& chunk, auto& accumulatedValue) {
                            for (const auto& sample : chunk.samples()) {
                                accumulatedValue.timestamps.record(
                                        sample.meta.timestampBitWidth);
                                accumulatedValue.values.record(
                                        sample.meta.valueBitWidth);
                            }
                        }),
                params);
    } else {
        display(accumulate<size_t>(
                        dirPath,
                        [](ChunkView& chunk, auto& accumulatedValue) {
                            accumulatedValue += chunk.dataLen;
                        }),
                params);
    }

    return 0;
}
