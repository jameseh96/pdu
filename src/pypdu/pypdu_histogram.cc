#include "pypdu_histogram.h"

#include <pdu/histogram/histogram.h>
#include <pdu/histogram/histogram_iterator.h>
#include <pdu/histogram/histogram_time_span.h>

#include <pybind11/stl.h>

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
            .def("__getitem__", [](const Histogram& th, size_t i) {
                return th.getValues().at(i);
            });

    py::class_<TimestampedHistogram, Histogram>(m, "TimepointHistogram")
            .def_property_readonly("timestamp",
                                   &TimestampedHistogram::getTimestamp);

    py::class_<HistogramTimeSpan>(m, "HistogramTimeSeries")
            .def_property_readonly("name", &HistogramTimeSpan::getName)
            .def_property_readonly("labels", &HistogramTimeSpan::getLabels)
            .def_property_readonly("buckets", &HistogramTimeSpan::getBuckets)
            .def("__len__", &HistogramTimeSpan::size)
            .def(
                    "__getitem__",
                    [](const HistogramTimeSpan& hts, int i) {
                        if (i < 0) {
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