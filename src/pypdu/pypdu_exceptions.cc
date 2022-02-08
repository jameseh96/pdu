#include "pypdu_exceptions.h"

#include <istream>

#include <pdu/exceptions.h>

void init_exceptions(py::module m) {
    // can register custom exception translators here, if needed
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p) {
                std::rethrow_exception(p);
            }
        } catch (const pdu::EOFError& e) {
            PyErr_SetString(PyExc_EOFError, e.what());
        }
    });
}
