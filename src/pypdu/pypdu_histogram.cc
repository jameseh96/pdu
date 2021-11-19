#include "pypdu_histogram.h"

#include <pdu/histogram/histogram.h>
#include <pdu/histogram/histogram_iterator.h>
#include <pdu/histogram/histogram_time_span.h>

#include <pybind11/operators.h>
#include <pybind11/stl.h>

#include <fmt/format.h>

void init_histogram(py::module_& m) {
    py::class_<Histogram>(m, "Histogram")
            .def("__len__",
                 [](const Histogram& th) { return th.getValues().size(); })
            .def(
                    "__iter__",
                    [](const Histogram& th) {
                        return py::make_iterator(th.getValues().begin(),
                                                 th.getValues().end());
                    },
                    py::keep_alive<0, 1>())
            .def("__getitem__",
                 [](const Histogram& th, size_t i) {
                     return th.getValues().at(i);
                 })
            .def("bucket_values", &Histogram::getValues)
            .def("bucket_bounds", &Histogram::getBounds)
            .def("buckets", [](const Histogram& hist) {
                const auto& values = hist.getValues();
                const auto& bounds = hist.getBounds();
                std::vector<std::pair<double, double>> buckets;
                if (values.size() != bounds.size()) {
                    throw std::runtime_error(
                            fmt::format("Histogram: different number of "
                                        "values:{} and boundaries:{} (bug)",
                                        values.size(),
                                        bounds.size()));
                }
                for (int i = 0; i < values.size(); ++i) {
                    buckets.emplace_back(bounds[i], values[i]);
                }
                return buckets;
            });

    py::class_<TimestampedHistogram, Histogram>(m, "TimepointHistogram")
            .def_property_readonly("timestamp",
                                   &TimestampedHistogram::getTimestamp)
            .def(py::self + py::self)
            .def(py::self - py::self);

    py::class_<DeltaHistogram, Histogram>(m, "DeltaHistogram")
            .def_property_readonly("time_delta", &DeltaHistogram::getTimeDelta);

    py::class_<HistogramTimeSpan>(m, "HistogramTimeSeries")
            .def_property_readonly("name", &HistogramTimeSpan::getName)
            .def_property_readonly("labels", &HistogramTimeSpan::getLabels)
            .def_property_readonly("bucket_bounds",
                                   &HistogramTimeSpan::getBounds)
            .def("__len__", &HistogramTimeSpan::size)
            .def(
                    "__getitem__",
                    [](const HistogramTimeSpan& hts, int i) {
                        if (i < 0) {
                            if (-i > hts.size()) {
                                throw py::index_error(fmt::format(
                                        "Negative index {} out of range for "
                                        "HistogramTimeSeries with size {}",
                                        i,
                                        hts.size()));
                            }
                            // allow negative indexing
                            i = i + hts.size();
                        }
                        return hts.at(i);
                    },
                    py::return_value_policy::reference_internal);

    py::class_<HistogramIterator>(m, "HistogramIterable")
            .def(
                    "__iter__",
                    [](const HistogramIterator& hi) {
                        return py::make_iterator<
                                py::return_value_policy::reference_internal,
                                HistogramIterator,
                                EndSentinel,
                                HistogramTimeSpan>(hi, end(hi));
                    },
                    py::keep_alive<0, 1>());
    ;
}