cmake_minimum_required(VERSION 3.17)
project(pypdu)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(../../third_party/pybind11 ${CMAKE_CURRENT_BINARY_DIR}/pybind11)
pybind11_add_module(pypdu pypdu.cc)

# EXAMPLE_VERSION_INFO is defined by setup.py and passed into the C++ code as a
# define (VERSION_INFO) here.
target_compile_definitions(pypdu PRIVATE VERSION_INFO=${EXAMPLE_VERSION_INFO})

target_link_libraries(pypdu PUBLIC plib)
