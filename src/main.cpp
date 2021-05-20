#include "chunk_file_cache.h"
#include "decoder.h"
#include "display_units.h"
#include "index.h"

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>

enum class SortOrder { Default, Size, Percentage };

std::istream& operator>>(std::istream& in, SortOrder& sort) {
    std::string token;
    in >> token;

    // modifies token
    boost::algorithm::to_lower(token);

    if (token == "default") {
        sort = SortOrder::Default;
    } else if (token == "size") {
        sort = SortOrder::Size;
    } else if (token == "percentage") {
        sort = SortOrder::Percentage;
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
            ("total,c", po::bool_switch(&summary), "Print total")
            ("human,h", po::bool_switch(&human), "Use \"human-readable\" units")
            ("percent,p", po::bool_switch(&percent), "Display percentage of total usage")
            ("sort,S", po::value(&sort), "Sort output, valid values: \"default\", \"size\", \"percentage\"")
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

Index loadIndex(std::string fname) {
    std::ifstream f(fname, std::ios_base::binary);
    f.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    Decoder indexDec(f);

    Index index;
    index.load(indexDec);
    return index;
}

void aggregate(std::map<std::string, size_t>& timeSeries,
               const Index& index,
               ChunkFileCache cache) {
    for (const auto& tableEntry : index.series) {
        const auto& series = tableEntry.second;
        const auto& labels = series.labels;

        std::string_view name = labels.at("__name__");

        auto itr = timeSeries.emplace(name, 0).first;

        for (const auto& chunk : series.chunks) {
            Decoder d{cache.get(chunk.getSegmentFileId())};
            d.seekg(chunk.getOffset());
            auto len = d.read_varuint();
            itr->second += len;
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

    std::map<std::string, size_t> timeSeries;

    for (const auto& file : fs::directory_iterator(dirPath)) {
        const auto& subdir = file.path();
        const auto& indexFile = subdir / "index";
        if (!fs::is_regular_file(indexFile)) {
            continue;
        }

        auto index = loadIndex(indexFile.string());

        ChunkFileCache cache(subdir / "chunks");

        aggregate(timeSeries, index, cache);
    }

    size_t total = 0;
    for (const auto& series : timeSeries) {
        total += series.second;
    }

    auto print = [total, &params](size_t value, std::string_view name) {
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
        fmt::print(" {}\n", name);
    };

    if (params.summary) {
        print(total, "total");
    }

    if (params.sort == SortOrder::Default) {
        for (const auto& [name, count] : timeSeries) {
            print(count, name);
        }
    } else {
        // name, size, percentage
        using Value = std::tuple<std::string_view, size_t, double>;
        std::vector<Value> values;
        for (const auto& [name, count] : timeSeries) {
            values.emplace_back(name, count, double(count * 100) / total);
        }

        std::function<bool(Value, Value)> comparator;

        if (params.sort == SortOrder::Size) {
            comparator = [](const auto& a, const auto& b) {
                return std::get<1>(a) < std::get<1>(b);
            };
        } else if (params.sort == SortOrder::Percentage) {
            comparator = [](const auto& a, const auto& b) {
                return std::get<2>(a) < std::get<2>(b);
            };
        } else {
            throw std::logic_error("Unknown sort order");
        }

        if (params.reverse) {
            comparator = [inner = std::move(comparator)](const auto& a,
                                                         const auto& b) {
                return inner(b, a);
            };
        }

        std::sort(values.begin(), values.end(), comparator);

        for (const auto& val : values) {
            print(std::get<1>(val), std::get<0>(val));
        }
    }

    return 0;
}
