#include "pypdu_expression.h"

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

    py::class_<ResamplingIterator>(m, "ResampledExpression")
            .def("__iter__", [](const ResamplingIterator& itr) {
                return py::make_iterator<py::return_value_policy::copy,
                                         ResamplingIterator,
                                         EndSentinel,
                                         Sample>(itr, EndSentinel());
            });

    m.def("irate",
          &irate,
          "Compute instantaneous rate of an expression (see Prometheus irate)");

    m.def(
            "irate",
            [](const CrossIndexSeries& cis) { return irate(cis); },
            "Compute instantaneous rate of an expression (see Prometheus "
            "irate)");
}