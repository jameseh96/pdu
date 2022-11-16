#include "pypdu_expression.h"

#include "pypdu_conversion_helpers.h"
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

    def_conversions(m, expressionClass);

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
}