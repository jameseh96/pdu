#pragma once

#include "pypdu.h"

#include <pdu/expression/expression.h>
#include <pdu/pdu.h>

#include <pybind11/operators.h>

template <class T, class... Args>
void enable_arithmetic_mutating(T& class_, Args&&... right) {
    (class_.def(py::self += right)
             .def(py::self -= right)
             .def(py::self *= right)
             .def(py::self /= right),
     ...);
}

template <class T, class U, class V>
void enable_arithmetic_non_mutating(T& class_, U left, V right) {
    class_.def(left + right)
            .def(left - right)
            .def(left * right)
            .def(left / right);
}

template <class T, class... Args>
void enable_arithmetic(T& class_, Args&&... args) {
    class_.def(-py::self);
    class_.def(+py::self);
    enable_arithmetic_non_mutating(class_, py::self, py::self);

    (enable_arithmetic_non_mutating(class_, py::self, args), ...);
    (enable_arithmetic_non_mutating(class_, args, py::self), ...);
}

void init_expression(py::module m);