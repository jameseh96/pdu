#include "series_filter.h"

#include <regex>

namespace pdu::filter {

Filter exactly(std::string expected) {
    return [expected](std::string_view value) { return value == expected; };
}
Filter regex(std::string expression) {
    std::regex expr(
            expression,
            std::regex_constants::ECMAScript | std::regex_constants::icase);
    return [expr = std::move(expr)](std::string_view value) {
        return std::regex_match(value.begin(), value.end(), expr);
    };
}

} // namespace pdu::filter

std::set<size_t> SeriesFilter::operator()(const Index& index) const {
    PerLabelRefs refs;

    if (empty()) {
        // no filters specified, collect all series IDs
        std::set<size_t> res;
        for (const auto& [k, v] : index.series) {
            res.insert(k);
        }
        return res;
    }


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

void SeriesFilter::operator()(const Index& index,
                              PerLabelRefs& seriesRefs) const {
    for (const auto& postingOffset : index.postings) {
        (*this)(postingOffset, index, seriesRefs);
    }
}

void SeriesFilter::operator()(PostingOffset po,
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
