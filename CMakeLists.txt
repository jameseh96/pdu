cmake_minimum_required(VERSION 3.17)
project(prometheus_disk_usage)

set(CMAKE_CXX_STANDARD 17)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

include(conan.cmake)

conan_cmake_configure(REQUIRES
        fmt/6.1.2
        boost/1.76.0
        nlohmann_json/3.9.1
        snappy/1.1.9
        GENERATORS cmake_find_package)

conan_cmake_autodetect(settings)

conan_cmake_install(PATH_OR_REFERENCE .
        BUILD missing
        REMOTE conan-center
        SETTINGS ${settings})

list(PREPEND CMAKE_MODULE_PATH "${CMAKE_BINARY_DIR}")

find_package(Boost REQUIRED)
find_package(fmt REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(Snappy REQUIRED)


add_library(plib
        src/pdu/pdu.cc
        src/pdu/io/decoder.cc
        src/pdu/io/index.cc
        src/pdu/io/chunk_file_cache.cc
        src/pdu/io/chunk_view.cc
        src/pdu/io/bit_decoder.cc
        src/pdu/io/mapped_file.cc
        src/pdu/io/index_iterator.cc
        src/pdu/display/bitwidth_histogram.cc
        src/pdu/io/resource.cc
        src/pdu/io/posting_offset_iterator.cc
        src/pdu/io/series_sample_iterator.cc
        src/pdu/query/filtered_index_iterator.cc
        src/pdu/query/series_filter.cc
        src/pdu/query/sample_visitor.cc
        src/pdu/query/series_iterator.cc
        src/pdu/query/cross_index_sample_iterator.cc)

target_include_directories(plib PUBLIC src/)
target_link_libraries(plib PUBLIC
        Boost::boost
        Boost::program_options
        Boost::filesystem
        fmt::fmt
        nlohmann_json::nlohmann_json
        Snappy::snappy)

set_property(TARGET plib PROPERTY POSITION_INDEPENDENT_CODE ON)

add_executable(pdu
        src/pdu_main.cpp
        )

add_executable(pdump
        src/pdump_main.cpp
        )

target_link_libraries(pdu PUBLIC plib)
target_link_libraries(pdump PUBLIC plib)

add_subdirectory(./src/pyprometheus)
