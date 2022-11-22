#include "pypdu_serialisation.h"

#include "pdu/block.h"
#include "pdu/encode.h"
#include "pdu/filter.h"
#include "pdu/pdu.h"
#include "pdu/serialisation/serialisation.h"

#include <boost/iostreams/device/file_descriptor.hpp>
#include <boost/iostreams/stream.hpp>
// using boost variant to allow targeting older MacOS before std::visit
// was available.
#include <boost/variant.hpp>

#include <sstream>
#include <utility>
#include <vector>

#include <pybind11/stl_bind.h>

#include "pypdu_boost_variant_helper.h"
#include "pypdu_exceptions.h"

int fdFromObj(py::object fileLike) {
    auto fdObj = fileLike.attr("fileno")();
    if (!py::isinstance<py::int_>(fdObj)) {
        throw std::invalid_argument(
                "fileLike.fileno() does not return an integer file descriptor");
    }
    return fdObj.cast<int>();
}

auto load(int fd, bool allowMmap = true) {
    py::gil_scoped_release release;
    if (allowMmap) {
        auto mappedResource = try_map_fd(fd);
        if (mappedResource) {
            // this fd does represent a file on disk, and has been mmapped
            return pdu::deserialise(mappedResource);
        }
    }

    // Note, empty files will fail to be mapped, and will lead to EOFError
    // being thrown below

    // this fd might be a pipe or socket, and can't be mmapped.
    // fall back to reading sequentially, and buffering data in memory.
    namespace io = boost::iostreams;
    io::stream_buffer<io::file_descriptor_source> fpstream(
            fd, boost::iostreams::never_close_handle);
    std::istream is(&fpstream);
    is.exceptions(std::istream::failbit | std::istream::badbit |
                  std::istream::eofbit);
    try {
        StreamDecoder d(is);
        return pdu::deserialise(d);
    } catch (const std::istream::failure& e) {
        if (is.eof()) {
            throw py::EOFError("reached end of file prematurely");
        }
        // other error, just rethrow and allow pybind to wrap in runtime error
        throw;
    }
}

auto load(py::object fileLike, bool allowMmap = true) {
    return load(fdFromObj(fileLike), allowMmap);
}

template <class T>
void dump(int fd, const T& value) {
    py::gil_scoped_release release;
    namespace io = boost::iostreams;
    io::stream_buffer<io::file_descriptor_sink> fpstream(
            fd, boost::iostreams::never_close_handle);
    std::ostream os(&fpstream);
    Encoder e(os);
    pdu::serialise(e, value);
}

template <class T>
void dumpToObj(py::object fileLike, const T& value) {
    dump(fdFromObj(fileLike), value);
}

template <class T>
py::bytes dumps(const T& value) {
    std::stringstream ss;
    {
        py::gil_scoped_release release;
        Encoder e(ss);
        pdu::serialise(e, value);
    }
    return py::bytes(ss.str());
}

std::vector<CrossIndexSeries> toSeriesVector();

std::vector<std::reference_wrapper<const CrossIndexSeries>> toSeriesVector(
        py::list list) {
    std::vector<std::reference_wrapper<const CrossIndexSeries>> series;
    for (const auto& obj : list) {
        if (py::isinstance<CrossIndexSeries>(obj)) {
            series.emplace_back(obj.cast<CrossIndexSeries&>());
        } else {
            throw py::type_error(
                    "Can only serialise lists if they contain only "
                    "Series objects");
        }
    }
    return series;
}

/**
 * Class holding the stream
 */
class StreamLoader {
public:
    StreamLoader(int fd)
        : fpstream(fd, boost::iostreams::never_close_handle),
          is(&fpstream),
          itr(is) {
    }

    StreamLoader(const StreamLoader&) = delete;
    StreamLoader(StreamLoader&&) = delete;

    StreamLoader& operator=(const StreamLoader&) = delete;
    StreamLoader& operator=(StreamLoader&&) = delete;

    boost::iostreams::stream_buffer<boost::iostreams::file_descriptor_source>
            fpstream;
    std::istream is;
    pdu::StreamIterator itr;
};

void def_serial(py::module m) {
    m.def("dump",
          &dump<CrossIndexSeries>,
          "Write a serialised representation of a Series to a file descriptor");
    m.def(
            "dump",
            [](int fd, py::list list) { dump(fd, toSeriesVector(list)); },
            "Write a serialised representation of a list of Series to a file "
            "descriptor");
    m.def("dump",
          &dump<PrometheusData>,
          "Write a serialised representation of all Series contained in a "
          "PrometheusData instance to a file descriptor");
    m.def("dump",
          &dump<SeriesIterator>,
          "Write a serialised representation of all Series in a (potentially "
          "filtered) iterator to a file descriptor");

    // dump to file-like object with .fileno()
    m.def("dump",
          &dumpToObj<CrossIndexSeries>,
          "Write a serialised representation of a Series to a file-like object "
          "supporting .fileno(), returning a file descriptor");
    m.def(
            "dump",
            [](py::object fileLike, py::list list) {
                dumpToObj(fileLike, toSeriesVector(list));
            },
            "Write a serialised representation of a list of Series to a "
            "file-like object supporting .fileno(), returning a file "
            "descriptor");
    m.def("dump",
          &dumpToObj<PrometheusData>,
          "Write a serialised representation of all Series contained in a "
          "PrometheusData instance to a file-like object supporting .fileno(), "
          "returning a file "
          "descriptor");
    m.def("dump",
          &dumpToObj<SeriesIterator>,
          "Write a serialised representation of all Series in a (potentially "
          "filtered) iterator to a file-like object supporting .fileno(), "
          "returning a file descriptor");

    m.def("dumps",
          &dumps<CrossIndexSeries>,
          "Write a serialised representation of a Series to bytes");
    m.def(
            "dumps",
            [](py::list list) { return dumps(toSeriesVector(list)); },
            "Write a serialised representation of a list of Series to bytes");
    m.def("dumps",
          &dumps<PrometheusData>,
          "Write a serialised representation of all Series contained in a "
          "PrometheusData instance to bytes");
    m.def("dumps",
          &dumps<SeriesIterator>,
          "Write a serialised representation of all Series in a (potentially "
          "filtered) iterator to bytes");

    py::class_<DeserialisedSeries, CrossIndexSeries>(m, "DeserialisedSeries");
    py::bind_vector<std::vector<DeserialisedSeries>>(
            m, "DeserialisedSeriesVector");

    py::class_<StreamLoader>(m, "LazyLoader")
            .def(
                    "__iter__",
                    [](const StreamLoader& sl) {
                        return py::make_iterator<py::return_value_policy::copy,
                                                 pdu::StreamIterator,
                                                 EndSentinel,
                                                 DeserialisedSeries>(
                                sl.itr, EndSentinel());
                    },
                    py::keep_alive<0, 1>());

    using namespace pybind11::literals;

    m.def("load",
          py::overload_cast<int, bool>(&load),
          "Load a Series from a serialised representation read from a file "
          "descriptor",
          "file_descriptor"_a,
          "allow_mmap"_a = true);
    m.def("load",
          py::overload_cast<py::object, bool>(&load),
          "Load a Series from a serialised representation read from a "
          "file-like object supporting .fileno(), returning a file "
          "descriptor",
          "file_descriptor"_a,
          "allow_mmap"_a = true);
    m.def(
            "loads",
            [](py::buffer buffer) {
                auto info = buffer.request();
                if (!PyBuffer_IsContiguous(info.view(), 'C')) {
                    throw std::runtime_error(
                            "pypdu.loads only accepts contiguous row-major (C "
                            "style) buffers");
                }
                if (info.ndim != 1) {
                    throw std::runtime_error(
                            "pypdu.loads only accepts one dimensional buffers");
                }
                if (info.format != py::format_descriptor<uint8_t>::format()) {
                    throw std::runtime_error(
                            "pypdu.loads only accepts one dimensional buffers "
                            "of bytes");
                }

                if (info.itemsize != 1) {
                    throw std::runtime_error(
                            "pypdu.loads only accepts one dimensional buffers "
                            "of bytes");
                }

                if (info.size < 0) {
                    throw std::runtime_error(
                            "pypdu.loads received invalid buffer");
                }
                if (info.size == 0) {
                    throw std::runtime_error(
                            "pypdu.loads received empty buffer");
                }

                Decoder d(reinterpret_cast<char*>(info.ptr), size_t(info.size));
                return pdu::deserialise(d);
            },
            py::keep_alive<0, 1>(),
            "Load a Series from a serialised representation read from a bytes "
            "object");

    m.def(
            "load_lazy",
            [](int fd) { return std::make_unique<StreamLoader>(fd); },
            "Lazily load one or more Series from a serialised representation "
            "read from a file descriptor");
    m.def(
            "load_lazy",
            [](py::object obj) {
                return std::make_unique<StreamLoader>(fdFromObj(obj));
            },
            "Lazily load one or more Series from a serialised representation "
            "read from a file-like object supporting .fileno(), returning a "
            "file descriptor");
}
