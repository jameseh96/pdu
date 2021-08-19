#include "pyprometheus.h"

#include <pdu/pdu.h>

#include <pybind11/operators.h>
#include <pybind11/stl.h>

int add(int i, int j) {
    return i + j;
}

PYBIND11_MODULE(pyprometheus, m) {
    m.doc() = "Python bindings to pdu, for reading Prometheus on-disk data";

    m.def("load",
          py::overload_cast<const std::string&>(&pdu::load),
          "Load data from a Prometheus data directory");

    py::class_<Sample>(m, "Sample")
            .def_readonly("timestamp", &Sample::timestamp)
            .def_readonly("value", &Sample::value)
            // support unpacking in the form of
            // for sample in samples:
            //     ...
            .def(
                    "__getitem__",
                    [](const Sample& sample, size_t i) {
                        if (i >= 2) {
                            throw py::index_error();
                        }
                        return i == 0 ? sample.timestamp : sample.value;
                    },
                    py::return_value_policy::copy)
            .def("__len__", []() { return 2; })
            // for nice presentation
            .def("__repr__",
                 [](const Sample& a) {
                     return "{timestamp=" + std::to_string(a.timestamp) +
                            ", value=" + std::to_string(a.value) + "}";
                 })
            .def(py::self == py::self)
            .def(py::self != py::self);

    py::class_<Series>(m, "Series")
            .def_property_readonly("name",
                                   [](const Series& series) {
                                       return series.labels.at("__name__");
                                   })
            .def_property_readonly("labels", [](const Series& series) {
                return series.labels;
            });

    py::class_<CrossIndexSeries>(m, "CrossIndexSeries")
            .def_property_readonly(
                    "series",
                    [](const CrossIndexSeries& cis) { return *cis.series; },
                    py::keep_alive<0, 1>())
            .def_property_readonly(
                    "samples",
                    [](const CrossIndexSeries& cis) {
                        return py::make_iterator<py::return_value_policy::copy,
                                                 CrossIndexSampleIterator,
                                                 EndSentinel,
                                                 Sample>(
                                cis.sampleIterator, end(cis.sampleIterator));
                    },
                    py::keep_alive<0, 1>())
            // support unpacking in the form of
            // for series, samples in data:
            //     ...
            .def(
                    "__getitem__",
                    [](const CrossIndexSeries& cis, size_t i) {
                        if (i >= 2)
                            throw py::index_error();
                        return i == 0 ? py::cast(*cis.series)
                                      : py::make_iterator<
                                                py::return_value_policy::copy,
                                                CrossIndexSampleIterator,
                                                EndSentinel,
                                                Sample>(
                                                cis.sampleIterator,
                                                end(cis.sampleIterator));
                    },
                    py::keep_alive<0, 1>())
            .def("__len__", []() { return 2; });

    py::class_<PrometheusData>(m, "PrometheusData")
                    .def(py::init<std::string>())
    // Allow iteration, default to unfiltered (all time series will be listed)
    .def(
            "__iter__",
            [](const PrometheusData& pd) {
                return py::make_iterator<py::return_value_policy::copy,
                                         SeriesIterator,
                                         EndSentinel,
                                         CrossIndexSeries>(pd.begin(), pd.end());
            },
            py::keep_alive<0, 1>() /* Essential: keep object alive while iterator exists */);
}