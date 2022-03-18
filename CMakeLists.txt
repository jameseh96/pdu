cmake_minimum_required(VERSION 3.17)
project(prometheus_disk_usage)

set(CMAKE_CXX_STANDARD 17)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

include(conan.cmake)

conan_cmake_configure(REQUIRES
        fmt/6.1.2
        boost/1.74.0
        nlohmann_json/3.9.1
        snappy/1.1.9
        gsl-lite/0.38.1
        GENERATORS cmake_find_package)

conan_cmake_autodetect(settings)

option(CONAN_MACOS_ARM64 "Dowload (or build) MacOS Arm64 deps from conan" OFF)

if(DEFINED ENV{CONAN_MACOS_ARM64})
    if($ENV{CONAN_MACOS_ARM64} MATCHES "ON")
        set(CONAN_MACOS_ARM64 ON)
    endif()
endif()

if (CONAN_MACOS_ARM64)
    set(CONAN_PROFILE_HOST_ARG "${PROJECT_SOURCE_DIR}/conan/profiles/armv8")
else()
    set(CONAN_PROFILE_HOST_ARG "")
endif()


conan_cmake_install(PATH_OR_REFERENCE .
        BUILD missing
        REMOTE conancenter
        SETTINGS ${settings}
        PROFILE_HOST ${CONAN_PROFILE_HOST_ARG})

list(PREPEND CMAKE_MODULE_PATH "${CMAKE_BINARY_DIR}")

find_package(Boost REQUIRED)
find_package(fmt REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(Snappy REQUIRED)
find_package(gsl-lite REQUIRED)


add_library(plib
        src/pdu/pdu.cc
        src/pdu/display/bitwidth_histogram.cc
        src/pdu/io/decoder.cc
        src/pdu/io/encoder.cc
        src/pdu/io/index.cc
        src/pdu/io/chunk_file_cache.cc
        src/pdu/io/chunk_reference.cc
        src/pdu/io/chunk_view.cc
        src/pdu/io/bit_decoder.cc
        src/pdu/io/head_chunks.cc
        src/pdu/io/mapped_file.cc
        src/pdu/io/index_iterator.cc
        src/pdu/io/resource.cc
        src/pdu/io/posting_offset_iterator.cc
        src/pdu/io/series_sample_iterator.cc
        src/pdu/io/wal.cc
        src/pdu/histogram/histogram.cc
        src/pdu/histogram/histogram_iterator.cc
        src/pdu/histogram/histogram_time_span.cc
        src/pdu/query/filtered_index_iterator.cc
        src/pdu/query/series_filter.cc
        src/pdu/query/sample_visitor.cc
        src/pdu/query/series_iterator.cc
        src/pdu/query/cross_index_sample_iterator.cc
        src/pdu/serialisation/deserialised_cross_index_series.cc
        src/pdu/serialisation/serialisation.cc
        src/pdu/util/host.cc)

set_property(TARGET plib PROPERTY CXX_VISIBILITY_PRESET hidden)

target_include_directories(plib PUBLIC src/)
target_link_libraries(plib PUBLIC
        Boost::boost
        Boost::program_options
        Boost::filesystem
        Boost::iostreams
        fmt::fmt
        nlohmann_json::nlohmann_json
        Snappy::snappy
        gsl::gsl-lite)

set_property(TARGET plib PROPERTY POSITION_INDEPENDENT_CODE ON)

add_executable(pdu
        src/pdu_main.cpp
        )

add_executable(pdump
        src/pdump_main.cpp
        )

target_link_libraries(pdu PUBLIC plib)
target_link_libraries(pdump PUBLIC plib)

add_subdirectory(./src/pypdu)
