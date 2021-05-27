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
#include <regex>

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
            ("bitwidth,b", po::bool_switch(&showBitwidth), "Display timestamp/value encoding bit width distributions")
            ("filter,f", po::value(&filter), "Regex filter applied to metric family names");

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
    std::string filter = "";
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
        auto [scaled, unit] = format::humanReadableBytes(totalSize / 8);
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

struct SampleData {
    BitWidthHistogram timestamps;
    BitWidthHistogram values;
    size_t usage = 0;

    SampleData& operator+=(const SampleData& other) {
        timestamps += other.timestamps;
        values += other.values;
        usage += other.usage;
        return *this;
    }
};

void printBytesUsed(std::string_view key,
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
        auto percent = total ? double(value * 100) / total : 100.0;
        fmt::print(" {:>7}", fmt::format("{:.2f}%", percent));
    }

    // print name
    fmt::print(" {}\n", key);
}

void printSampleHistograms(std::string_view key,
                           const SampleData& hists,
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
void display(const std::map<std::string, SampleData, std::less<>>& data,
             const params_t& params) {
    // sum up all values if total or percentage required
    SampleData total{};
    if (params.total || params.percent) {
        for (const auto& series : data) {
            total += series.second;
        }
    }

    auto print = [total, &params](auto&& key, const SampleData& value) {
        if (params.showBitwidth) {
            printSampleHistograms(key, value, params);
        } else {
            printBytesUsed(key, value.usage, total.usage, params);
        }
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
        std::vector<Value> nameAndSize;
        for (const auto& [key, value] : data) {
            nameAndSize.emplace_back(key, value.usage);
        }

        auto comparator = makeComparator(params.sort, params.reverse);

        std::sort(nameAndSize.begin(), nameAndSize.end(), comparator);

        for (const auto& val : nameAndSize) {
            auto name = std::get<0>(val);
            print(name, data.find(name)->second);
        }
    }
}

int main(int argc, char* argv[]) {
    params_t params(argc, argv);
    if (!params.valid) {
        return 1;
    }

    namespace fs = boost::filesystem;

    std::function<bool(const std::string&)> filter;

    if (params.filter.empty()) {
        filter = [](const std::string& name) { return true; };
    } else {
        filter = [re = std::regex(params.filter)](const std::string& name) {
            return bool(std::regex_match(name, re));
        };
    }

    fs::path dirPath = params.statsDir;

    // std::less<> allows for hetrogenous lookup
    std::map<std::string, SampleData, std::less<>> perSeriesValues;

    auto findSeries = [&perSeriesValues](const std::string& name) {
        auto itr = perSeriesValues.find(name);

        if (itr == perSeriesValues.end()) {
            itr = perSeriesValues.try_emplace(name, SampleData{}).first;
        }

        return itr;
    };

    // iterate over every chunk index in the provided directory
    for (const auto& [index, subdir] : IndexIterator(dirPath)) {
        // Once a chunk file reference is encountered in the index, the
        // appropriate chunk file will be mmapped and inserted into the cache
        // as they are likely to be used again.
        ChunkFileCache cache(subdir / "chunks");

        // iterate over each time series in the index
        for (const auto& tableEntry : index.series) {
            const auto& series = tableEntry.second;
            auto name = std::string(series.labels.at("__name__"));

            if (!filter(name)) {
                continue;
            }

            auto& data = findSeries(name)->second;

            for (const auto& chunk : series.chunks) {
                // ChunkView parses the chunk start info, but will not read
                // all samples unless they are iterated over.
                ChunkView view(cache, chunk);
                data.usage += view.dataLen;

                if (params.showBitwidth) {
                    for (const auto& sample : view.samples()) {
                        data.timestamps.record(sample.meta.timestampBitWidth);
                        data.values.record(sample.meta.valueBitWidth);
                    }
                }
            }
        }
    }

    display(perSeriesValues, params);

    return 0;
}
