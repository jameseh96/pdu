#include "pypdu_numpy_check.h"

bool numpy_available_inner(py::module m) {
    try {
        pybind11::module_::import("numpy");
        return true;
    } catch (...) {
        return false;
    }
}

bool numpy_available(py::module m) {
    static bool available = numpy_available_inner(m);
    return available;
}