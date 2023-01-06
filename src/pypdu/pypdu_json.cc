#include "pypdu_json.h"

#include "pypdu_expression.h"
#include "pypdu_series_samples.h"

#include "pdu/block/sample.h"

#include <fmt/format.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <string_view>

using namespace rapidjson;

template <class SampleRange>
void samples_to_json(Writer<StringBuffer>& writer, const SampleRange& samples) {
    writer.StartArray();

    for (const auto& s : samples) {
        writer.StartArray();
        writer.Int64(s.timestamp);
        writer.Double(s.value);
        writer.EndArray();
    }

    writer.EndArray();
}

void series_to_json(Writer<StringBuffer>& writer, const CrossIndexSeries& cis) {
    writer.StartObject();

    writer.Key("metric");
    writer.StartObject();
    for (const auto& [k, v] : cis.getLabels()) {
        writer.Key(k.data(), k.size());
        writer.String(v.data(), v.size());
    }
    writer.EndObject();

    writer.Key("values");
    samples_to_json(writer, cis.getSamples());

    writer.EndObject();
}

void dispatch(Writer<StringBuffer>& writer, const py::handle& obj) {
    // pypdu types
    if (py::isinstance<SeriesSamples>(obj)) {
        samples_to_json(writer, obj.cast<const SeriesSamples&>().getIterator());
    } else if (py::isinstance<Expression>(obj)) {
        samples_to_json(writer, obj.cast<const Expression&>());
    } else if (py::isinstance<std::vector<Sample>>(obj)) {
        samples_to_json(writer, obj.cast<const std::vector<Sample>&>());
    } else if (py::isinstance<CrossIndexSeries>(obj)) {
        series_to_json(writer, obj.cast<const CrossIndexSeries&>());
    }
    // built-in python non-aggregate types
    else if (obj.is_none()) {
        writer.Null();
    } else if (py::isinstance<bool>(obj)) {
        writer.Bool(obj.cast<bool>());
    } else if (py::isinstance<py::str>(obj) || py::isinstance<py::bytes>(obj)) {
        auto sv = obj.cast<std::string_view>();
        writer.String(sv.data(), sv.size());
    } else if (py::isinstance<py::int_>(obj)) {
        writer.Uint64(obj.cast<uint64_t>());
    } else if (py::isinstance<py::float_>(obj)) {
        writer.Double(obj.cast<double>());
    }
    // built-in python aggregate types
    else if (py::isinstance<py::dict>(obj)) {
        writer.StartObject();
        for (const auto& [k, v] : obj.cast<py::dict>()) {
            dispatch(writer, k);
            dispatch(writer, v);
        }
        writer.EndObject();
    } else if (py::isinstance<py::iterable>(obj)) {
        writer.StartArray();
        for (const auto& v : obj.cast<py::iterable>()) {
            dispatch(writer, v);
        }
        writer.EndArray();
    } else {
        throw py::type_error(fmt::format(
                "pypdu.json: Object of type {} is not JSON serializable",
                obj.attr("__class__").attr("__name__").cast<std::string>()));
    }
}

py::object to_json(const py::object& obj) {
    using namespace rapidjson;
    StringBuffer sb;
    Writer<StringBuffer> writer(sb);

    dispatch(writer, obj);
#if PYTHON_VERSION_MAJOR == 2
    return py::bytes(sb.GetString(), sb.GetSize());
#else
    return py::str(sb.GetString(), sb.GetSize());
#endif
}

void init_json(py::module_& mainModule) {
    py::module_ m = mainModule.def_submodule("json");

    m.def("dumps", &to_json);
}