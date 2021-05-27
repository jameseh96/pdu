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
            ("total,c", po::bool_switch(&summary), "Print total")
            ("human,h", po::bool_switch(&human), "Use \"human-readable\" units")
            ("percent,p", po::bool_switch(&percent), "Display percentage of total usage")
            ("sort,S", po::value(&sort), "Sort output, valid values: \"default\", \"size\"")
            ("reverse,r", po::bool_switch(&reverse), "Reverse sort order");

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
    }
    std::string statsDir = "";
    bool summary = false;
    bool human = false;
    bool percent = false;
    SortOrder sort = SortOrder::Default;
    bool reverse = false;
    bool valid = false;
};

/**
 * Write a collection of key-value pairs to stdout in a configurable, du-esque
 * format.
 */
template <class T>
void display(const T& data, const params_t& params) {
    // sum up all values if total or percentage required
    typename T::mapped_type total{};
    if (params.summary || params.percent) {
        for (const auto& series : data) {
            total += series.second;
        }
    }

    auto print = [total, &params](auto&& key, auto&& value) {
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
    };

    if (params.summary) {
        print("total", total);
    }

    if (params.sort == SortOrder::Default) {
        for (const auto& [key, value] : data) {
            print(key, value);
        }
    } else {
        std::vector<Value> values;
        for (const auto& [key, value] : data) {
            values.emplace_back(key, value);
        }

        auto comparator = makeComparator(params.sort, params.reverse);

        std::sort(values.begin(), values.end(), comparator);

        for (const auto& val : values) {
            print(std::get<0>(val), std::get<1>(val));
        }
    }
}

int main(int argc, char* argv[]) {
    params_t params(argc, argv);
    if (!params.valid) {
        return 1;
    }

    namespace fs = boost::filesystem;

    fs::path dirPath = params.statsDir;

    // std::less<> allows for hetrogenous lookup
    std::map<std::string, size_t, std::less<>> timeSeriesUsage;

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

            auto itr = timeSeriesUsage.find(name);

            if (itr == timeSeriesUsage.end()) {
                itr = timeSeriesUsage.try_emplace(std::string(name), 0).first;
            }

            for (const auto& chunk : series.chunks) {
                // ChunkView parses the chunk start info, but will not read
                // all samples unless they are iterated over.
                ChunkView view(cache, chunk);
                // accumulate the total chunk file bytes used by a given series
                itr->second += view.dataLen;
            }
        }
    }

    display(timeSeriesUsage, params);

    return 0;
}
