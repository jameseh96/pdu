#pragma once

#include "../io/index.h"

#include <functional>
#include <map>
#include <set>
#include <string_view>
#include <utility>

namespace pdu::filter {
using Filter = std::function<bool(std::string_view)>;

Filter exactly(std::string expected);
Filter regex(std::string expression);

} // namespace pdu::filter

class SeriesFilter {
public:
    using ValueMatcher = std::function<bool(std::string_view)>;
    using PerLabelRefs = std::map<std::string_view, std::set<size_t>>;

    void addFilter(std::string_view key, ValueMatcher valueMatcher) {
        matchers.try_emplace(std::string(key), std::move(valueMatcher));
    }

    void addFilter(std::string_view key, std::string value) {
        using namespace pdu::filter;
        addFilter(key, exactly(std::move(value)));
    }

    std::set<size_t> operator()(const Index& index) const;

    bool operator()(const Series& series) const;

    bool empty() const {
        return matchers.empty();
    }

private:
    void operator()(const Index& index, PerLabelRefs& seriesRefs) const;

    void operator()(PostingOffset po,
                    const Index& index,
                    PerLabelRefs& seriesRefs) const;

    std::map<std::string, ValueMatcher, std::less<>> matchers;
};