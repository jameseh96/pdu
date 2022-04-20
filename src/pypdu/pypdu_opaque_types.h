#pragma once

#include <pybind11/cast.h>

#include <vector>

class DeserialisedSeries;
PYBIND11_MAKE_OPAQUE(std::vector<DeserialisedSeries>);

class Sample;
PYBIND11_MAKE_OPAQUE(std::vector<Sample>);
