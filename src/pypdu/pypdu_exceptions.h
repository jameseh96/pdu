#pragma once

#include "pypdu.h"

namespace pybind11 {
// insert EOFError into the pybind namespace, for convenience.
// EOFError could be set manually with PyErr_SetString, but would need to
// remember to reacquire the GIL.
PYBIND11_RUNTIME_EXCEPTION(EOFError, PyExc_EOFError)
} // namespace pybind11

void init_exceptions(py::module m);