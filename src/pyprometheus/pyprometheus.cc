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

    // note - this is intentionally inconsistent naming to better reflect
    // the fact that in normal usage __iter__ will be called on this type
    // to get a python iterator, despite this being a C++ iterator already.
    py::class_<CrossIndexSampleIterator>(m, "CrossIndexSampleIterable")
            .def(
                    "__len__",
                    [](const CrossIndexSampleIterator& cisi) {
                        return cisi.getNumSamples();
                    },
                    py::return_value_policy::copy)
            .def(
                    "__iter__",
                    [](const CrossIndexSampleIterator& cisi) {
                        return py::make_iterator<py::return_value_policy::copy,
                                                 CrossIndexSampleIterator,
                                                 EndSentinel,
                                                 Sample>(cisi, end(cisi));
                    },
                    py::keep_alive<0, 1>());

    py::class_<CrossIndexSeries>(m, "Series")
            .def_property_readonly(
                    "name",
                    [](const CrossIndexSeries& cis) {
                        if (!cis.series) {
                            throw std::runtime_error(
                                    "Can't get name, series is invalid");
                        }
                        return cis.series->labels.at("__name__");
                    },
                    py::keep_alive<0, 1>())
            .def_property_readonly(
                    "labels",
                    [](const CrossIndexSeries& cis) {
                        if (!cis.series) {
                            throw std::runtime_error(
                                    "Can't get labels, series is invalid");
                        }
                        return cis.series->labels;
                    },
                    py::keep_alive<0, 1>())
            .def_property_readonly(
                    "samples",
                    [](const CrossIndexSeries& cis) {
                        return cis.sampleIterator;
                    },
                    py::keep_alive<0, 1>())
            // support unpacking in the form of
            // for series, samples in data:
            //     ...
            .def("__getitem__",
                 [](const CrossIndexSeries& cis, size_t i) {
                     if (!cis.series) {
                         throw std::runtime_error(
                                 "Can't unpack, series is invalid");
                     }
                     switch (i) {
                     case 0:
                         return py::cast(cis.series->labels.at("__name__"));
                     case 1:
                         return py::cast(cis.series->labels);
                     case 2:
                         auto ret = py::cast(cis.sampleIterator);
                         // manually set up keep alive here, as it is not
                         // possible to do so for the name and labels, so it
                         // cannot be set as a policy for the method.
                         keep_alive_impl(ret, py::cast(cis));
                         return ret;
                     }
                     throw py::index_error();
                 })
            .def("__len__", []() { return 3; });

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