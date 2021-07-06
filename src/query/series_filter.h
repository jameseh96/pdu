#pragma once

#include "io/index.h"

#include <functional>
#include <map>
#include <set>
#include <string_view>

class SeriesFilter {
public:
    using ValueMatcher = std::function<bool(std::string_view)>;

    void addFilter(std::string_view key, ValueMatcher valueMatcher) {
        matchers.try_emplace(std::string(key), std::move(valueMatcher));
    }

    void operator()(const Index& index) {
        for (const auto& postingOffset : index.postings) {
            (*this)(postingOffset, index);
        }
    }

    void operator()(PostingOffset po, const Index& index) {
        if (auto itr = matchers.find(po.labelKey); itr != matchers.end()) {
            const auto& [labelKey, matcher] = *itr;
            if (matcher(po.labelValue)) {
                // collect up all references to series which match this
                // specific label matcher
                for (const auto& seriesRef : index.getSeriesRefs(po)) {
                    seriesRefs[labelKey].insert(seriesRef);
                }
            }
        }
    }

    std::set<size_t> getResult() const {
        if (seriesRefs.empty()) {
            return {};
        }
        auto itr = seriesRefs.begin();
        std::set<size_t> result = itr->second;
        std::set<size_t> tmp;
        ++itr;
        for (; itr != seriesRefs.end(); ++itr) {
            auto& perLabel = itr->second;
            // intersect the series which matched on each label to find
            // only the series which matched all label filters
            std::set_intersection(result.begin(),
                                  result.end(),
                                  perLabel.begin(),
                                  perLabel.end(),
                                  std::inserter(tmp, tmp.begin()));
            std::swap(result, tmp);
            tmp.clear();
        }

        return result;
    }

    void clear() {
        seriesRefs.clear();
    }

private:
    std::map<std::string, ValueMatcher, std::less<>> matchers;
    std::map<std::string_view, std::set<size_t>> seriesRefs;
};