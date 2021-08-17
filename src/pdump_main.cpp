#include "io/index.h"
#include "io/index_iterator.h"
#include "query/filtered_index_iterator.h"
#include "query/sample_visitor.h"
#include "query/series_filter.h"

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <fstream>
#include <iostream>
#include <memory>
#include <regex>
#include <vector>

struct params_t {
    params_t(int argc, char* argv[]) {
        namespace po = boost::program_options;

        boost::program_options::options_description options("Allowed options");
        boost::program_options::positional_options_description pos_options;

        // clang-format off
        options.add_options()
            ("dir,d", po::value(&statsDir)->required(), "Prometheus stats directory")
            ("query,q", po::value(&query), "Prometheus query");

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
                    "    pdump <options> <dir>\n"
                    "e.g.,\n"
                    "    pdump -q '{foo=\"bar\"}' ./stats_data\n\n");
            fmt::print("{}\n", options);
            valid = false;
        }
    }
    std::string statsDir = "";
    std::string query = "";
    bool valid = false;
};

std::vector<std::shared_ptr<Index>> loadIndexes(
        const boost::filesystem::path& dataDir) {
    std::vector<std::shared_ptr<Index>> indexes;

    for (auto indexPtr : IndexIterator(dataDir)) {
        indexes.push_back(indexPtr);
    }
    return indexes;
}

std::vector<FilteredIndexIterator> loadFilteredIndexes(
        const boost::filesystem::path& dataDir, const SeriesFilter& filter) {
    std::vector<FilteredIndexIterator> filteredIndexes;

    for (auto indexPtr : IndexIterator(dataDir)) {
        filteredIndexes.emplace_back(indexPtr, filter);
    }
    return filteredIndexes;
}

int main(int argc, char* argv[]) {
    // parse command line args
    params_t params(argc, argv);
    if (!params.valid) {
        // if parsing failed, a usage message will already have been printed.
        // just exit now.
        return 1;
    }

    return 0;
}
