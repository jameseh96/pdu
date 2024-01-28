#include "pypdu_conversion_helpers.h"

#include "pdu/block/sample.h"

#include <numeric>

/**
 * Modify provided samples to meet requested timestamp units and NaN filtering.
 *
 * Timestamps may be scaled to seconds if required, and NaN values may be
 * removed from the vector if @p filterNaNValues is true.
 */
void maybeConvertOrFilter(std::vector<Sample>& samples,
                          TimestampUnits units,
                          bool filterNaNValues) {
    if (filterNaNValues) {
        samples.erase(std::remove_if(samples.begin(),
                                     samples.end(),
                                     [](const Sample& sample) {
                                         return std::isnan(sample.value);
                                     }),
                      samples.end());
    }
    if (units == TimestampUnits::Seconds) {
        for (Sample& s : samples) {
            s.timestamp /= 1000;
        }
    }
}

/**
 * Modify provided sample to meet requested timestamp units and NaN filtering.
 *
 * Timestamp may be scaled to seconds if required. Returns true if sample
 * should be retained, false if it should be skipped (i.e., is NaN and NaN
 * filtering is requested).
 *
 * @param sample
 * @param units
 * @param filterNaNValues
 * @return whether the sample should be kept
 */
bool maybeConvertOrFilter(Sample& sample,
                          TimestampUnits units,
                          bool filterNaNValues) {
    if (filterNaNValues) {
        return !std::isnan(sample.value);
    }
    if (units == TimestampUnits::Seconds) {
        sample.timestamp /= 1000;
    }
    return true;
}

std::vector<Sample> to_samples(const SeriesSamples& ss) {
    return ss.getSamples();
}

std::vector<Sample> to_samples(const Expression& expr) {
    std::vector<Sample> samples;
    for (const auto& sample : expr) {
        samples.push_back(sample);
    }
    return samples;
}

template <class SampleSource>
void def_conversions(py::module m, py::class_<SampleSource>& cls) {
    using namespace pybind11::literals;
    cls.def(
               "as_vector",
               [](const SampleSource& ss,
                  TimestampUnits units,
                  bool filterNaNValues) {
                   auto samples = to_samples(ss);
                   maybeConvertOrFilter(samples, units, filterNaNValues);
                   return samples;
               },
               "timestamp_units"_a = TimestampUnits::Milliseconds,
               "filter_nan_values"_a = false,
               "Get a read-only list-like view of these samples")
            .def(
                    "as_list",
                    [](const SampleSource& ss,
                       TimestampUnits units,
                       bool filterNaNValues) {
                        auto samples = to_samples(ss);

                        maybeConvertOrFilter(samples, units, filterNaNValues);

                        py::list l(samples.size());
                        for (int i = 0; i < samples.size(); i++) {
                            const auto& sample = samples[i];
                            py::list elem(2);
                            elem[0] = sample.timestamp;
                            elem[1] = sample.value;
                            l[i] = std::move(elem);
                        }
                        return l;
                    },
                    "timestamp_units"_a = TimestampUnits::Milliseconds,
                    "filter_nan_values"_a = false);

    if (numpy_available(m)) {
        cls.def(
                "as_array",
                [](const SampleSource& ss,
                   TimestampUnits units,
                   bool filterNaNValues) {
                    auto samples = to_samples(ss);

                    maybeConvertOrFilter(samples, units, filterNaNValues);

                    auto size = samples.size();
                    auto data = samples.data();

                    return py::array_t(
                            size, data, py::cast(std::move(samples)));
                },
                "timestamp_units"_a = TimestampUnits::Milliseconds,
                "filter_nan_values"_a = false);
    } else {
        cls.def("as_array",
                [](const SampleSource& ss,
                   py::args args,
                   const py::kwargs& kwargs) {
                    // allow calls with any args/kwargs, just want to give
                    // an exception.
                    throw std::runtime_error(
                            "Accessing samples as a numpy array requires numpy "
                            "to be installed");
                });
    }
}

template void def_conversions(py::module m, py::class_<SeriesSamples>& cls);
template void def_conversions(py::module m, py::class_<Expression>& cls);