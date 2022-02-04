#pragma once

#include <pybind11/cast.h>

#include <vector>

class DeserialisedSeries;
PYBIND11_MAKE_OPAQUE(std::vector<DeserialisedSeries>);

class RawSample;
PYBIND11_MAKE_OPAQUE(std::vector<RawSample>);