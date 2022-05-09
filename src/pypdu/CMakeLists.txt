cmake_minimum_required(VERSION 3.17)
project(pypdu)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(../../third_party/pybind11 ${CMAKE_CURRENT_BINARY_DIR}/pybind11)
pybind11_add_module(pypdu
        pypdu.cc
        pypdu_histogram.cc
        pypdu_serialisation.cc
        pypdu_version.cc
        pypdu_exceptions.cc
        pypdu_expression.cc)

set_property(TARGET pypdu PROPERTY CXX_VISIBILITY_PRESET hidden)

# EXAMPLE_VERSION_INFO is defined by setup.py and passed into the C++ code as a
# define (VERSION_INFO) here.
target_compile_definitions(pypdu PRIVATE VERSION_INFO=${EXAMPLE_VERSION_INFO})

target_link_libraries(pypdu PUBLIC plib)


# determine git revision
execute_process(COMMAND git log --pretty=format:'%h' -n 1
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_REV
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE)


if ("${GIT_REV}" STREQUAL "")
    set(GIT_REV "N/A")
else()
    execute_process(
            COMMAND git describe --exact-match --tags
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            OUTPUT_VARIABLE GIT_TAG
            ERROR_QUIET
            OUTPUT_STRIP_TRAILING_WHITESPACE)

    string(SUBSTRING "${GIT_REV}" 1 7 GIT_REV)
    string(STRIP "${GIT_TAG}" GIT_TAG)
endif()

if ("${PYPDU_VERSION}" STREQUAL "")
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/VERSION.txt PYPDU_VERSION)
string(STRIP "${PYPDU_VERSION}" PYPDU_VERSION)
endif()

set(VERSION "const char* VERSION=\"${PYPDU_VERSION}\";
const char* GIT_REV=\"${GIT_REV}\";
const char* GIT_TAG=\"${GIT_TAG}\";")

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/generated_version.cc)
    file(READ ${CMAKE_CURRENT_SOURCE_DIR}/generated_version.cc VERSION_)
else()
    set(VERSION_ "")
endif()

if (NOT "${VERSION}" STREQUAL "${VERSION_}")
    file(WRITE ${CMAKE_CURRENT_SOURCE_DIR}/generated_version.cc "${VERSION}")
endif()
