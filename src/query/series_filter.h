#pragma once

#include "io/index.h"

#include <functional>
#include <map>
#include <set>
#include <string_view>

class SeriesFilter {
public:
    using ValueMatcher = std::function<bool(std::string_view)>;
    using PerLabelRefs = std::map<std::string_view, std::set<size_t>>;

    void addFilter(std::string_view key, ValueMatcher valueMatcher) {
        matchers.try_emplace(std::string(key), std::move(valueMatcher));
    }

    std::set<size_t> operator()(const Index& index) const {
        PerLabelRefs refs;

        // collect all series references by label key matching a provided
        // matcher.
        (*this)(index, refs);

        if (refs.empty()) {
            return {};
        }

        // find the intersection of the references for each label key
        // e.g., to match
        //     {__name__=~"foo.*", job="bar"}
        // intersect all references to series which match the __name__ selector
        // with those which match the job selector - the resulting set of
        // references point to series matching the filter.
        auto itr = refs.begin();
        std::set<size_t> result = itr->second;
        std::set<size_t> tmp;
        ++itr;
        for (; itr != refs.end(); ++itr) {
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

private:
    void operator()(const Index& index, PerLabelRefs& seriesRefs) const {
        for (const auto& postingOffset : index.postings) {
            (*this)(postingOffset, index, seriesRefs);
        }
    }

    void operator()(PostingOffset po,
                    const Index& index,
                    PerLabelRefs& seriesRefs) const {
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

    std::map<std::string, ValueMatcher, std::less<>> matchers;
};