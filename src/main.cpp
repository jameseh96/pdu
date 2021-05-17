#include "decoder.h"
#include "index.h"

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <fstream>
#include <map>
#include <memory>

struct params_t {
    params_t(int argc, char* argv[]) {
        namespace po = boost::program_options;

        boost::program_options::options_description options("Allowed options");
        boost::program_options::positional_options_description pos_options;

        // clang-format off
        options.add_options()
            ("dir,d", po::value(&statsDir)->required(), "Prometheus stats directory");

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
            fmt::print("{}\n{}\n", e.what(), options);
            valid = false;
        }
    }
    std::string statsDir = "";
    bool valid = false;
};

Index loadIndex(std::string fname) {
    std::ifstream f(fname, std::ios_base::binary);
    Decoder indexDec(f);

    Index index;
    index.load(indexDec);
    return index;
}

class ChunkFileCache {
public:
    ChunkFileCache(boost::filesystem::path chunkDir)
        : chunkDir(std::move(chunkDir)) {
    }
    std::ifstream& get(uint32_t segmentId) {
        if (auto itr = cache.find(segmentId); itr != cache.end()) {
            return *itr->second;
        }

        auto path = chunkDir / fmt::format("{:0>6}", segmentId);
        if (!boost::filesystem::is_regular_file(path)) {
            throw std::runtime_error(
                    fmt::format("Index references missing chunk file: {}\n",
                                path.string()));
        }
        auto& ptr = cache[segmentId];
        ptr = std::make_shared<std::ifstream>(path.string(),
                                              std::ios_base::binary);

        return *ptr;
    }

private:
    const boost::filesystem::path chunkDir;
    // using copyable shared_ptr as std::map refused being constructed
    // with a non-copyable type (buggy?)
    std::map<uint32_t, std::shared_ptr<std::ifstream>> cache;
};

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

    for (const auto& [name, count] : timeSeries) {
        fmt::print("{:<8} : {}\n", count, name);
    }

    return 0;
}
