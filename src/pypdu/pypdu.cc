#include "pypdu.h"

#include "pypdu_conversion_helpers.h"
#include "pypdu_exceptions.h"
#include "pypdu_expression.h"
#include "pypdu_histogram.h"
#include "pypdu_json.h"
#include "pypdu_numpy_check.h"
#include "pypdu_serialisation.h"
#include "pypdu_series_samples.h"
#include "pypdu_version.h"

#include <pdu/block/chunk_builder.h>
#include <pdu/histogram/histogram_iterator.h>
#include <pdu/pdu.h>

#include <pybind11/functional.h>
#include <pybind11/numpy.h>
#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

#include <optional>
#include <typeindex>

// Wrapper for a filter returned by a C++ method (e.g., pdu::filter::regex)
// to avoid overheads of calling through pybind. This distinguishes the
// callback from one which is genuinely python code.
struct WrappedFilter {
    WrappedFilter(const WrappedFilter&) = delete;
    WrappedFilter(WrappedFilter&&) = default;
    pdu::filter::Filter filter;
};

SeriesFilter makeFilter(const py::dict& dict) {
    SeriesFilter f;
    for (const auto& [kobj, vobj] : dict) {
        std::string k = py::str(kobj);
        if (py::isinstance<py::function>(vobj)) {
            // arbitrary python callback. Convert it to a Filter, which will
            // call back into Python on every evaluation (probably slow)
            f.addFilter(k, vobj.cast<pdu::filter::Filter>());
        } else if (py::isinstance<WrappedFilter>(vobj)) {
            // is a filter created in C++, get a reference to the real type
            f.addFilter(k, vobj.cast<WrappedFilter&>().filter);
        } else if (py::str(vobj)) {
            // is a plain old string, will filter to an exact match on this
            // label
            f.addFilter(k, std::string(py::str(vobj)));
        } else {
            throw std::invalid_argument(
                    "Filter only handles strings, regexes, and unary "
                    "funcs");
        }
    }
    return f;
}

/**
 * Make a filter from a single string.
 *
 * String is treated as an exact metric family name (__name__) to match.
 */
SeriesFilter makeFilter(const py::str& s) {
    SeriesFilter f;
    f.addFilter("__name__", std::string(s));
    return f;
}

/**
 * Make a filter from a single unary python function
 *
 * String is treated as a callable to match against a metric family name.
 */
SeriesFilter makeFilter(const pdu::filter::Filter& callable) {
    SeriesFilter f;
    f.addFilter("__name__", callable);
    return f;
}

/**
 * Make a filter from a single unary python function
 *
 * String is treated as a callable (regex or arbitrary python func) to match
 * against a metric family name.
 */
SeriesFilter makeFilter(const WrappedFilter& wf) {
    SeriesFilter f;
    f.addFilter("__name__", wf.filter);
    return f;
}

auto getFirstMatching(const PrometheusData& pd, const SeriesFilter& f) {
    auto itr = pd.filtered(f);
    if (itr == pd.end()) {
        throw py::key_error("No item matching filter");
    }
    return *itr;
}

template <class T>
auto getFirstMatching(const PrometheusData& pd, const T& val) {
    return getFirstMatching(pd, makeFilter(val));
}

/**
 * Type bundling a chunk view with a min/max time.
 * Chunks don't hold the min/max time, the ref in the index does.
 * This is a convenience type for python to expose the times without
 * passing refs around.
 */
struct PyChunk {
    ChunkView view;
    uint64_t minTime = 0;
    uint64_t maxTime = std::numeric_limits<int64_t>::max();
};

void makeXORPyChunks(const ChunkView& cv,
                     uint64_t minTime,
                     uint64_t maxTime,
                     std::vector<PyChunk>& out) {
    if (cv.isXOR()) {
        out.push_back({cv, minTime, maxTime});
        return;
    }
    ChunkBuilder builder;
    for (const auto& sample : cv.samples()) {
        builder.append(sample);
    }
    for (const auto& chunk : builder.finalise()) {
        // if one non-xor chunk became multiple xor chunks,
        // the min/max time will be inaccurate for each
        // chunk, but that's not too important for now.
        // If necessary, ChunkBuilder::append could indicate if a new
        // chunk has been started so minTime could be changed.
        out.push_back({chunk, minTime, maxTime});
    }
}

PYBIND11_MODULE(pypdu, m) {
    m.doc() = "Python bindings to pdu, for reading Prometheus on-disk data";

    py::enum_<TimestampUnits>(m, "TimestampUnits")
            .value("Milliseconds", TimestampUnits::Milliseconds)
            .value("Seconds", TimestampUnits::Seconds)
            .export_values();

    if (numpy_available(m)) {
        PYBIND11_NUMPY_DTYPE(Sample, timestamp, value);
        // the above dtype decl will attempt to import numpy.core.multiarray
        // but registering the type is required for pybind to use buffer
        // protocol.
    } else {
        // if numpy is not available, fall back to a hacky manual registration
        // just to bypass this. memoryview(series.samples.as_vector())
        // returns a view with 16 byte elements which could be unpacked with
        //  struct.unpack("qd", element)
        auto tindex = std::type_index(typeid(Sample));
        py::detail::get_numpy_internals().registered_dtypes[tindex];

        // an alternative would be to bind without buffer protocol in
        // this case, but that's inconvenient.
    }
    py::bind_vector<std::vector<Sample>>(
            m, "SampleVector", py::buffer_protocol());

    init_version(m);
    init_histogram(m);
    init_exceptions(m);
    init_expression(m);
    init_json(m);

    m.def("load",
          py::overload_cast<const std::string&>(&pdu::load),
          "Load data from a Prometheus data directory",
          py::call_guard<py::gil_scoped_release>());

    m.def(
            "regex",
            [](std::string expression) {
                return WrappedFilter{pdu::filter::regex(expression)};
            },
            "Specify a regular expression for a Filter to match against "
            "label "
            "values");

    py::class_<WrappedFilter>(m, "FilterFunc");

    py::class_<SeriesFilter>(m, "Filter")
            .def(py::init<>())
            .def(py::init<>(
                    [](const py::dict& dict) { return makeFilter(dict); }))
            .def("add",
                 [](SeriesFilter& f,
                    const std::string& labelKey,
                    const std::string& labelValue) {
                     f.addFilter(labelKey, labelValue);
                 })
            .def(
                    "addRegex",
                    [](SeriesFilter& f,
                       const std::string& labelKey,
                       const std::string& labelValue) {
                        f.addFilter(labelKey, pdu::filter::regex(labelValue));
                    },
                    "Add a label filter which matches values against an "
                    "ECMAScript regex")
            .def("is_empty", [](const SeriesFilter& f) { return f.empty(); });

    py::class_<Sample>(m, "Sample")
            .def(py::init<int64_t, double>())
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
                        return i == 0 ? py::cast(sample.timestamp)
                                      : py::cast(sample.value);
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

    py::class_<SampleInfo, Sample>(m, "SampleInfo");

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
                                                 SampleInfo>(cisi, end(cisi));
                    },
                    py::keep_alive<0, 1>());

    auto seriesSamplesClass =
            py::class_<SeriesSamples>(m, "SeriesSamples")
                    .def(
                            "__iter__",
                            [](const SeriesSamples& ss) {
                                return py::make_iterator<
                                        py::return_value_policy::copy,
                                        CrossIndexSampleIterator,
                                        EndSentinel,
                                        SampleInfo>(ss.getIterator(),
                                                    EndSentinel());
                            },
                            py::keep_alive<0, 1>())
                    .def(
                            "__len__",
                            [](const SeriesSamples& ss) {
                                return ss.getIterator().getNumSamples();
                            },
                            py::return_value_policy::copy);

    def_conversions(m, seriesSamplesClass);

    py::class_<PyChunk>(m, "Chunk", py::buffer_protocol())
            .def_buffer([](const PyChunk& pc) -> py::buffer_info {
                auto data = pc.view.data();
                // expose as buffer of raw bytes
                return py::buffer_info(
                        const_cast<char*>(data.data()), /* Pointer to buffer */
                        1, /* Size of one scalar */
                        py::format_descriptor<
                                uint8_t>::format(), /* Python struct-style
                                                       format descriptor */
                        1, /* Number of dimensions */
                        {data.size()}, /* Buffer dimensions */
                        {1});
            })
            .def_static(
                    "from_xor_bytes",
                    [](py::buffer buffer) {
                        auto info = buffer.request();

                        if (!PyBuffer_IsContiguous(info.view(), 'C')) {
                            throw std::runtime_error(
                                    "Chunk.from_xor_bytes only accepts "
                                    "contiguous "
                                    "row-major (C "
                                    "style) buffers");
                        }
                        if (info.ndim != 1) {
                            throw std::runtime_error(
                                    "Chunk.from_xor_bytes only accepts one "
                                    "dimensional "
                                    "buffers");
                        }
                        if (info.format !=
                            py::format_descriptor<uint8_t>::format()) {
                            throw std::runtime_error(
                                    "Chunk.from_xor_bytes only accepts one "
                                    "dimensional "
                                    "buffers "
                                    "of bytes");
                        }

                        if (info.itemsize != 1) {
                            throw std::runtime_error(
                                    "Chunk.from_xor_bytes only accepts one "
                                    "dimensional "
                                    "buffers "
                                    "of bytes");
                        }

                        if (info.size < 0) {
                            throw std::runtime_error(
                                    "Chunk.from_xor_bytes received invalid "
                                    "buffer");
                        }
                        if (info.size == 0) {
                            throw std::runtime_error(
                                    "Chunk.from_xor_bytes received empty "
                                    "buffer");
                        }

                        // todo: may be worth computing the min
                        //       and max time on demand. Not all usages will
                        //       require this, so it does not seem worth
                        //       parsing the samples unconditionally to find
                        //       the min and max ahead of time.
                        return PyChunk{ChunkView(
                                std::make_shared<MemResource>(std::string_view(
                                        reinterpret_cast<char*>(info.ptr),
                                        size_t(info.size))),
                                0,
                                ChunkType::XORData)};
                    },
                    py::keep_alive<0, 1>(),
                    "Create a Chunk from XOR encoded data")
            .def_static(
                    "from_samples",
                    [](py::buffer buffer) {
                        auto info = buffer.request();

                        if (!PyBuffer_IsContiguous(info.view(), 'C')) {
                            throw std::runtime_error(
                                    "Chunk.from_samples only accepts "
                                    "contiguous "
                                    "row-major (C "
                                    "style) buffers");
                        }
                        std::string commonMessage =
                                "Chunk.from_samples only accepts one "
                                "dimensional buffers of bytes "
                                "(dtype='uint8') or Samples "
                                "(dtype=[('timestamp', '<i8'), ('value', "
                                "'<f8')])";
                        if (info.ndim != 1) {
                            throw std::runtime_error(commonMessage);
                        }

                        // info.format _should_ be checked here, but it is
                        // not practical to exact string match with
                        // py::format_descriptor<...>::format()
                        // for complex types
                        // (q vs l depending on platform, pybind specifies
                        // ^ for unaligned), and field names are included
                        // but not important (requiring the timestamp field
                        // be named "timestamp" is unnecessarily restrictive
                        // given the data itself just needs to be int64_t

                        if (info.itemsize != 1 &&
                            info.itemsize !=
                                    (sizeof(int64_t) + sizeof(double))) {
                            throw std::runtime_error(
                                    commonMessage + ", not elements of size: " +
                                    std::to_string(info.itemsize));
                        }

                        if (info.size < 0) {
                            throw std::runtime_error(
                                    "Chunk.from_samples received invalid "
                                    "buffer (size < 0)");
                        }

                        std::vector<PyChunk> chunks;
                        auto cv = ChunkView(
                                std::make_shared<MemResource>(std::string_view(
                                        reinterpret_cast<char*>(info.ptr),
                                        size_t(info.size * info.itemsize))),
                                0,
                                ChunkType::Raw);

                        makeXORPyChunks(cv, 0, 0, chunks);
                        return chunks;
                    },
                    "Create a Chunk from raw samples (e.g., from an array)")
            .def_property(
                    "min_time",
                    [](const PyChunk& pc) { return int64_t(pc.minTime); },
                    [](PyChunk& pc, int64_t value) { pc.minTime = value; })
            .def_property(
                    "max_time",
                    [](const PyChunk& pc) { return int64_t(pc.maxTime); },
                    [](PyChunk& pc, int64_t value) { pc.maxTime = value; })
            .def(
                    "samples",
                    [](const PyChunk& pc) {
                        std::vector<Sample> samples;
                        samples.reserve(pc.view.numSamples());
                        for (const auto& sample : pc.view.samples()) {
                            samples.push_back(sample);
                        }
                        return samples;
                    },
                    "Return a vector of samples contained in this chunk")
            .def(
                    "_first_sample",
                    [](const PyChunk& pc) {
                        auto itr = pc.view.samples();
                        if (itr == end(itr)) {
                            throw std::runtime_error(
                                    "Chunk::_first_sample called on empty or "
                                    "invalid chunk");
                        }
                        return *itr;
                    },
                    "Return the first sample from this chunk (used for "
                    "validation of min/max time)")
            .def(
                    "as_bytes",
                    [](const PyChunk& pc) {
                        auto data = pc.view.xor_data();
                        return py::bytes(data.data(), data.size());
                    },
                    "Returns a copy of the raw XOR encoded chunk data as a "
                    "bytes object")
            .def(
                    "view",
                    [](const py::object& pv) {
                        // takes a py::object to allow pybind to construct
                        // a memory view based on the above buffer_info
                        return py::memoryview(pv);
                    },
                    "Returns a memoryview over the raw XOR encoded chunk data, "
                    "without copying");

    auto seriesClass =
            py::class_<CrossIndexSeries>(m, "Series")
                    .def_property_readonly(
                            "name",
                            [](const CrossIndexSeries& cis) {
                                if (!cis) {
                                    throw std::runtime_error(
                                            "Can't get name, series is "
                                            "invalid");
                                }
                                return cis.getLabels().at("__name__");
                            },
                            py::keep_alive<0, 1>())
                    .def_property_readonly(
                            "labels",
                            [](const CrossIndexSeries& cis) {
                                if (!cis) {
                                    throw std::runtime_error(
                                            "Can't get labels, series is "
                                            "invalid");
                                }
                                return cis.getLabels();
                            },
                            py::keep_alive<0, 1>())
                    .def_property_readonly(
                            "samples",
                            [](const CrossIndexSeries& cis) {
                                return SeriesSamples(cis.getSamples());
                            })
                    .def_property_readonly("chunks",
                                           [](const CrossIndexSeries& cis) {
                                               std::vector<PyChunk> chunks;
                                               for (const auto& [ref, chunk] :
                                                    cis.getChunks()) {
                                                   makeXORPyChunks(chunk,
                                                                   ref.minTime,
                                                                   ref.maxTime,
                                                                   chunks);
                                               }
                                               return chunks;
                                           })

                    // support unpacking in the form of
                    // for series, samples in data:
                    //     ...
                    .def("__getitem__",
                         [](const CrossIndexSeries& cis, size_t i) {
                             if (!cis) {
                                 throw std::runtime_error(
                                         "Can't unpack, series is invalid");
                             }
                             switch (i) {
                             case 0:
                                 return py::cast(
                                         cis.getLabels().at("__name__"));
                             case 1:
                                 return py::cast(cis.getLabels());
                             case 2:
                                 auto ret = py::cast(
                                         SeriesSamples(cis.getSamples()));
                                 // manually set up keep alive here, as it is
                                 // not possible to do so for the name and
                                 // labels, so it cannot be set as a policy for
                                 // the method.
                                 keep_alive_impl(ret, py::cast(cis));
                                 return ret;
                             }
                             throw py::index_error();
                         })
                    .def("__len__", []() { return 3; });

    enable_arithmetic(seriesClass, float());

    py::class_<SeriesIterator>(m, "SeriesIterator")
            .def(
                    "__iter__",
                    [](const SeriesIterator& si) {
                        return py::make_iterator<py::return_value_policy::copy,
                                                 SeriesIterator,
                                                 EndSentinel,
                                                 CrossIndexSeries>(
                                si, EndSentinel());
                    },
                    py::keep_alive<0, 1>());

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
        py::keep_alive<0, 1>() /* Essential: keep object alive while iterator exists */)
        // Allow iteration, default to unfiltered (all time series will be listed)
    .def(
        "filter",
        [](const PrometheusData& pd, const SeriesFilter& f) {
            return pd.filtered(f);
        },
        py::keep_alive<0, 1>() /* Essential: keep object alive while iterator exists */)
    .def(
        "filter",
        [](const PrometheusData& pd, const py::dict& dict) {
            return pd.filtered(makeFilter(dict));
        },
        py::keep_alive<0, 1>())
    .def(
        "filter",
        [](const PrometheusData& pd, const py::str& s) {
            return pd.filtered(makeFilter(s));
        },
        py::keep_alive<0, 1>())
    // support an arbitrary python callback as a __name__ filter
    .def(
        "filter",
        [](const PrometheusData& pd, const pdu::filter::Filter& f) {
            return pd.filtered(makeFilter(f));
        },
        py::keep_alive<0, 1>())
    // support a C++ constructed filter (avoiding it being treated as a
    // python callback)
    .def(
        "filter",
        [](const PrometheusData& pd, const WrappedFilter& f) {
            return pd.filtered(makeFilter(f));
        },
        py::keep_alive<0, 1>())
    .def("__getitem__", [](const PrometheusData& pd, const SeriesFilter& f) {
        return getFirstMatching(pd, f);
        }, py::keep_alive<0, 1>())
    .def("__getitem__", [](const PrometheusData& pd, const py::dict& dict) {
        return getFirstMatching(pd, dict);
    }, py::keep_alive<0, 1>())
    .def("__getitem__", [](const PrometheusData& pd, const py::str& s) {
        return getFirstMatching(pd, s);
        }, py::keep_alive<0, 1>())
        // support an arbitrary python callback as a __name__ filter
    .def("__getitem__", [](const PrometheusData& pd, const pdu::filter::Filter& f) {
        return getFirstMatching(pd, f);
    }, py::keep_alive<0, 1>())
    // support a C++ constructed filter (avoiding it being treated as a
    // python callback)
    .def("__getitem__", [](const PrometheusData& pd, const WrappedFilter& f) {
        return getFirstMatching(pd, f);
    }, py::keep_alive<0, 1>())
    .def_property_readonly(
        "histograms",
        &PrometheusData::getHistograms,
    py::keep_alive<0, 1>());

    def_serial(m);
}