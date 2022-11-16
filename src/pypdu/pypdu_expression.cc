#include "pypdu_expression.h"

#include "pypdu_numpy_check.h"

#include <pybind11/numpy.h>
#include <pybind11/stl.h>

void init_expression(py::module m) {
    auto expressionClass =
            py::class_<Expression>(m, "Expression")
                    .def(py::init<CrossIndexSeries>())
                    .def(py::init<double>())
                    .def(
                            "__iter__",
                            [](const Expression& expr) {
                                return py::make_iterator<
                                        py::return_value_policy::copy,
                                        ExpressionIterator,
                                        EndSentinel,
                                        Sample>(expr.begin(), EndSentinel());
                            },
                            py::keep_alive<0, 1>())
                    .def(
                            "resample",
                            [](const Expression& expr, int interval) {
                                if (interval <= 0) {
                                    throw std::runtime_error(
                                            "resample requires a positive "
                                            "number of "
                                            "milliseconds as the interval");
                                }
                                return expr.resample(
                                        std::chrono::milliseconds(interval));
                            },
                            "Resample an expression with the given interval "
                            "(milliseconds) from the first sample. Does not "
                            "interpolate.");

    enable_arithmetic(expressionClass, float(), CrossIndexSeries{});
    enable_arithmetic_mutating(expressionClass, py::self);
    enable_arithmetic_mutating(expressionClass, float());

    py::implicitly_convertible<CrossIndexSeries, Expression>();

    py::class_<ResamplingIterator>(m, "ResampledExpression")
            .def("__iter__", [](const ResamplingIterator& itr) {
                return py::make_iterator<py::return_value_policy::copy,
                                         ResamplingIterator,
                                         EndSentinel,
                                         Sample>(itr, EndSentinel());
            });

    using namespace pybind11::literals;
    m.def("irate",
          &irate,
          "expression"_a,
          "monotonic"_a = false,
          "Compute instantaneous rate of an expression (see Prometheus irate). "
          "Mimics Prometheus rate handling of counter reset if monotonic=True "
          "(to avoids a large negative rate by calculating rate as if previous "
          "sample was zero)");

    m.def("sum",
          &Expression::sum,
          "Compute the sum of a list of series (equivalent to standard `sum`, "
          "but potentially faster)");

    m.def("resample",
          &resample,
          "Resample a series at the given interval. Where the new sample does "
          "not align with an existing sample, the value will be linearly "
          "interpolated");

    if (numpy_available(m)) {
        expressionClass.def(
                "as_array",
                [](const Expression& expr) {
                    auto samples = std::make_unique<std::vector<Sample>>();
                    for (const auto& sample : expr) {
                        samples->push_back(sample);
                    }
                    auto* ptr = samples.get();
                    // create an array backed by the samples. No copy needed,
                    // samples will be freed when the array is no longer
                    // referenced.
                    return py::array_t(ptr->size(),
                                       ptr->data(),
                                       py::cast(std::move(samples)));
                },
                py::keep_alive<0, 1>());
    } else {
        expressionClass.def("as_array", [](const Expression& expr) {
            throw std::runtime_error(
                    "Accessing expression results as a numpy array requires "
                    "numpy to be installed");
        });
    }
}