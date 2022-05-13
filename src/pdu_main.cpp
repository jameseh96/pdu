#include "pdu/display.h"
#include "pdu/io.h"
#include "pdu/query.h"

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

enum class SortOrder { Default, Size, AvgSize, Count };

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
    } else if (token == "count") {
        sort = SortOrder::Count;
    } else {
        in.setstate(std::ios_base::failbit);
    }
    return in;
}

struct params_t {
    params_t() = default;
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
            ("count,C", po::bool_switch(&count), "Display number of samples")
            ("avg,a", po::bool_switch(&average), "Display average (mean) sample size")
            ("percent,p", po::bool_switch(&percent), "Display percentage of total usage")
            ("sort,S", po::value(&sort), "Sort output, valid values: \"default\", \"size\", \"avgSize\", \"count\"")
            ("reverse,r", po::bool_switch(&reverse), "Reverse sort order")
            ("bitwidth,b", po::bool_switch(&showBitwidth), "Display timestamp/value encoding bit width distributions")
            ("minbitwidth,m", po::bool_switch(&showMinBitwidth),
                "Display minimum possible timestamp encoding bit width distributions (implies -b)")
            ("filter,f", po::value(&filter), "Regex (ECMASCript) filter applied to metric family names");

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
        if (showMinBitwidth) {
            showBitwidth = true;
        }
    }
    std::string statsDir = "";
    bool total = false;
    bool summary = false;
    bool human = false;
    bool count = false;
    bool average = false;
    bool percent = false;
    SortOrder sort = SortOrder::Default;
    bool reverse = false;
    bool showBitwidth = false;
    bool showMinBitwidth = false;
    std::string filter = "";
    bool valid = false;
};

struct AccumulatedData {
    BitWidthHistogram minTimestamps;
    BitWidthHistogram timestamps;
    BitWidthHistogram values;
    uint64_t diskUsage = 0;
    uint64_t sampleCount = 0;

    double avgSampleSize() const {
        return double(diskUsage) / sampleCount;
    }

    AccumulatedData& operator+=(const AccumulatedData& other) {
        minTimestamps += other.minTimestamps;
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

    if (params.count) {
        fmt::print(" {:>9}", value.sampleCount);
        if (params.percent) {
            auto percent = getPercent(value.sampleCount, total.sampleCount);
            fmt::print(" {:>7.2f}%", percent);
        }
    }

    // print name
    fmt::print("  {}\n", key);
}

/**
 * Print column headers.
 *
 * Formatting should match elements in printAggData
 */
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

    if (params.count) {
        fmt::print(" {:>9}", "Count");
        if (params.percent) {
            fmt::print(" {:>7}%", "Count");
        }
    }

    // print name
    fmt::print("  {}\n", "MetricFamily");
}

void printSampleHistograms(std::string_view key,
                           const AccumulatedData& hists,
                           const params_t& params) {
    // print name
    fmt::print("{}\n", key);

    if (params.showMinBitwidth) {
        fmt::print("  Min Timestamp Bits\n");
        hists.minTimestamps.print(params.percent, params.human);
    }

    fmt::print("  Timestamps\n");
    hists.timestamps.print(params.percent, params.human);

    fmt::print("  Values\n");
    hists.values.print(params.percent, params.human);
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
    } else if (params.sort == SortOrder::AvgSize) {
        comparator = [](const auto& a, const auto& b) {
            return a.second.get().avgSampleSize() <
                   b.second.get().avgSampleSize();
        };
    } else {
        comparator = [](const auto& a, const auto& b) {
            return a.second.get().sampleCount < b.second.get().sampleCount;
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
    if (!params.showBitwidth) {
        displayHeader(params);
    }

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
    // parse command line args
    params_t params(argc, argv);
    if (!params.valid) {
        // if parsing failed, a usage message will already have been printed.
        // just exit now.
        return 1;
    }

    namespace fs = boost::filesystem;

    std::function<bool(const std::string&)> filter;

    // create the filter - either accept everything or accept metric families
    // matching the provided regex
    if (params.filter.empty()) {
        filter = [](const std::string& name) { return true; };
    } else {
        filter = [re = std::regex(params.filter,
                                  std::regex_constants::ECMAScript)](
                         const std::string& name) {
            return bool(std::regex_match(name, re));
        };
    }

    fs::path dirPath = params.statsDir;

    // std::less<> allows for hetrogenous lookup
    std::map<std::string, AccumulatedData, std::less<>> perSeriesValues;

    // helper to get the accumulated data for a given metric family
    auto findSeries = [&perSeriesValues](const std::string& name) {
        auto itr = perSeriesValues.find(name);

        if (itr == perSeriesValues.end()) {
            itr = perSeriesValues.try_emplace(name, AccumulatedData{}).first;
        }

        return itr;
    };

    // iterate over every chunk index in the provided directory
    // see index.h/cc for index file parsing. IndexIterator
    // just locates every index file and loads it.
    for (auto indexPtr : IndexIterator(dirPath)) {
        const auto& index = *indexPtr;
        fs::path subdir = index.getDirectory();

        // Once a chunk file reference is encountered in the index, the
        // appropriate chunk file will be mmapped and inserted into the cache
        // as they are likely to be used again.
        ChunkFileCache cache(subdir / "chunks");

        // iterate over each time series in the index
        for (const auto& tableEntry : index.series) {
            const auto& series = tableEntry.second;
            auto name = std::string(series.labels.at("__name__"));

            // a regex filter may have been specified. Skip any non-matching
            // metric families
            if (!filter(name)) {
                continue;
            }

            // get the value from the name->AccumulatedData map to
            // store read data into
            auto& acc = findSeries(name)->second;

            // series in the index file specify references to chunk files
            // by segment id and offset.
            for (const auto& chunk : series.chunks) {
                // ChunkView parses the chunk start info, but will not read
                // all samples unless they are iterated over.
                ChunkView view(cache, chunk);
                acc.diskUsage += view.dataLen;
                acc.sampleCount += view.sampleCount;

                // iterating every sample is somewhat expensive, so only
                // do so if needed for the requested output
                if (params.showBitwidth) {
                    for (const auto& sample : view.samples()) {
                        if (sample.meta.minTimestampBitWidth !=
                            SampleInfo::noBitWidth) {
                            acc.minTimestamps.record(
                                    sample.meta.minTimestampBitWidth);
                        }
                        acc.timestamps.record(sample.meta.timestampBitWidth);
                        acc.values.record(sample.meta.valueBitWidth);
                    }
                }
            }
        }
    }

    // finally, display the accumulated data according to the specified
    // parameters
    display(perSeriesValues, params);

    return 0;
}
