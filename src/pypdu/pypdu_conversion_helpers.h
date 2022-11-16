#pragma once

#include "pypdu_numpy_check.h"
#include "pypdu_series_samples.h"

#include "pdu/expression/expression.h"

#include <pybind11/numpy.h>

#include <vector>

class Sample;

// units for pre-transforming timestamps as_array()
enum class TimestampUnits { Milliseconds = 0, Seconds };

template <class SampleSource>
void def_conversions(py::module m, py::class_<SampleSource>& cls);

extern template void def_conversions(py::module m,
                                     py::class_<SeriesSamples>& cls);

extern template void def_conversions(py::module m,
                                     py::class_<Expression>& cls);