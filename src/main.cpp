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

enum class SortOrder { Default, Size, AvgSize };

std::istream& operator>>(std::istream& in, SortOrder& sort) {
    std::string token;
    in >> token;

    // modifies token
    boost::algorithm::to_lower(token);

    if (token == "default") {
        sort = SortOrder::Default;
    } else if (token == "size") {
        sort = SortOrder::Size;
    } else if (token == "avgSize") {
        sort = SortOrder::AvgSize;
    } else {
        in.setstate(std::ios_base::failbit);
    }
    return in;
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
            ("avg,a", po::bool_switch(&average), "Display average (mean) sample size")
            ("percent,p", po::bool_switch(&percent), "Display percentage of total usage")
            ("sort,S", po::value(&sort), "Sort output, valid values: \"default\", \"size\", \"avgSize\"")
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
    bool average = false;
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

    size_t count() const {
        size_t total = 0;
        for (const auto& pair : values) {
            total += pair.second;
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

struct AccumulatedData {
    BitWidthHistogram timestamps;
    BitWidthHistogram values;
    size_t diskUsage = 0;
    size_t sampleCount = 0;

    double avgSampleSize() const {
        return double(diskUsage) / sampleCount;
    }

    AccumulatedData& operator+=(const AccumulatedData& other) {
        timestamps += other.timestamps;
        values += other.values;
        diskUsage += other.diskUsage;
        sampleCount += other.sampleCount;
        return *this;
    }
};

double getPercent(double value, double total) {
    return total ? double(value * 100) / total : 100.0;
}

void printAggData(std::string_view key,
                  AccumulatedData value,
                  AccumulatedData total,
                  const params_t& params) {
    // print the value
    if (params.human) {
        auto [scaled, unit] = format::humanReadableBytes(value.diskUsage);
        fmt::print("{:<7}", fmt::format("{}{}", scaled, unit));
    } else {
        fmt::print("{:<7}", value.diskUsage);
    }

    // maybe print a percentage disk usage of the total
    if (params.percent) {
        auto percent = getPercent(value.diskUsage, total.diskUsage);
        fmt::print(" {:>7.2f}%", percent);
    }

    // maybe print the average sample size
    if (params.average) {
        fmt::print(" {:>7.2f}B", value.avgSampleSize());

        // maybe print the percentage of the overall average sample size
        // useful for determining which time series are taking more bytes
        // the overall average to encode each sample
        if (params.percent) {
            auto percent =
                    getPercent(value.avgSampleSize(), total.avgSampleSize());
            fmt::print(" {:>7.2f}%", percent);
        }
    }

    auto getVal = [average = params.average](const auto& accData) -> double {
        if (average) {
            return accData.avgSampleSize();
        } else {
            return accData.diskUsage;
        }
    };

    // print name
    fmt::print("  {}\n", key);
}

void printSampleHistograms(std::string_view key,
                           const AccumulatedData& hists,
                           const params_t& params) {
    // print name
    fmt::print("{}\n", key);

    fmt::print("  Timestamps\n");
    printHistogram(hists.timestamps, params.percent, params.human);

    fmt::print("  Values\n");
    printHistogram(hists.values, params.percent, params.human);
}

// name and accumulated data
using Value = std::pair<std::string_view,
                        std::reference_wrapper<const AccumulatedData>>;
std::function<bool(Value, Value)> makeComparator(const params_t& params) {
    if (params.sort == SortOrder::Default) {
        throw std::logic_error(
                "Bug: Shouldn't be creating a comparator for the default mode");
    }
    std::function<bool(Value, Value)> comparator;
    if (params.sort == SortOrder::Size) {
        comparator = [](const auto& a, const auto& b) {
            return a.second.get().diskUsage < b.second.get().diskUsage;
        };
    } else {
        comparator = [](const auto& a, const auto& b) {
            return a.second.get().avgSampleSize() <
                   b.second.get().avgSampleSize();
        };
    }

    if (params.reverse) {
        comparator = [inner = std::move(comparator)](const auto& a,
                                                     const auto& b) {
            return inner(b, a);
        };
    }
    return comparator;
}

void displayHeader(const params_t& params) {
    // Value
    fmt::print("{:<7}", "Disk");

    // maybe print a percentage disk usage of the total
    if (params.percent) {
        fmt::print(" {:>7}%", "Disk");
    }

    // maybe print the average sample size
    if (params.average) {
        fmt::print(" {:>8}", "AvgSamp");

        // maybe print the percentage of the overall average sample size
        // useful for determining which time series are taking more bytes
        // the overall average to encode each sample
        if (params.percent) {
            fmt::print(" {:>7}%", "AvgSamp");
        }
    }
    // print name
    fmt::print("  {}\n", "MetricFamily");
}

/**
 * Write a collection of key-value pairs to stdout in a configurable, du-esque
 * format.
 */
void display(const std::map<std::string, AccumulatedData, std::less<>>& data,
             const params_t& params) {
    // sum up all values if total or percentage required
    AccumulatedData total{};
    if (params.total || params.percent) {
        for (const auto& series : data) {
            total += series.second;
        }
    }

    auto print = [total, &params](auto&& key, const AccumulatedData& value) {
        if (params.showBitwidth) {
            printSampleHistograms(key, value, params);
        } else {
            printAggData(key, value, total, params);
        }
    };

    // print header
    displayHeader(params);

    if (params.total) {
        print("<<Total>>", total);
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
            nameAndSize.emplace_back(key, value);
        }

        auto comparator = makeComparator(params);

        std::sort(nameAndSize.begin(), nameAndSize.end(), comparator);

        for (const auto& [name, value] : nameAndSize) {
            print(name, value);
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
    std::map<std::string, AccumulatedData, std::less<>> perSeriesValues;

    auto findSeries = [&perSeriesValues](const std::string& name) {
        auto itr = perSeriesValues.find(name);

        if (itr == perSeriesValues.end()) {
            itr = perSeriesValues.try_emplace(name, AccumulatedData{}).first;
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
                data.diskUsage += view.dataLen;
                data.sampleCount += view.sampleCount;

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
