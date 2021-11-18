#include "pypdu_version.h"

// include generated version file, created from VERSION.txt and git info
#include "generated_version.cc"

#include <fmt/format.h>
#include <fmt/ostream.h>
#include <ostream>
#include <sstream>
#include <string>
#include <utility>

struct Version {
    int major;
    int minor;
    int patch;
    std::string prerelease = "";
};

std::ostream& operator<<(std::ostream& os, const Version& v) {
    fmt::print(os, "{}.{}.{}{}", v.major, v.minor, v.patch, v.prerelease);
    return os;
}

bool operator<(const Version& a, const Version& b) {
    return std::tie(a.major, a.minor, a.patch, a.prerelease) <
           std::tie(b.major, b.minor, b.patch, b.prerelease);
}

Version get_version() {
    Version v;
    std::stringstream ss(VERSION);

    ss >> v.major;
    ss.ignore(1);
    ss >> v.minor;
    ss.ignore(1);
    ss >> v.patch;

    if (!ss.eof()) {
        ss >> v.prerelease;
    }
    return v;
}

void init_version(py::module_& m) {
    m.attr("__version__") = VERSION;
    m.attr("__git_rev__") = GIT_REV;
    m.attr("__git_tag__") = GIT_TAG;

    using namespace pybind11::literals;
    m.def(
            "require",
            [](int major, int minor, int patch, std::string prerelease = "") {
                Version required{major, minor, patch, prerelease};
                Version current = get_version();
                if (current < required) {
                    throw std::runtime_error(
                            fmt::format("Current pypdu version {} does not "
                                        "meet required {}",
                                        current,
                                        required));
                }
            },
            // default args
            "major"_a = 0,
            "minor"_a = 0,
            "patch"_a = 0,
            "prerelease"_a = "");
}