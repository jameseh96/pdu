cmake_minimum_required(VERSION 3.17)
project(prometheus_disk_usage)

set(CMAKE_CXX_STANDARD 17)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

include(conan.cmake)

conan_cmake_configure(REQUIRES
        fmt/6.1.2
        boost/1.76.0
        GENERATORS cmake_find_package)

conan_cmake_autodetect(settings)

conan_cmake_install(PATH_OR_REFERENCE .
        BUILD missing
        REMOTE conan-center
        SETTINGS ${settings})

list(PREPEND CMAKE_MODULE_PATH "${CMAKE_BINARY_DIR}")

find_package(Boost REQUIRED)
find_package(fmt REQUIRED)


add_library(plib
        src/io/decoder.cc
        src/io/index.cc
        src/io/chunk_file_cache.cc
        src/io/chunk_view.cc
        src/io/bit_decoder.cc
        src/io/mapped_file.cc
        src/io/index_iterator.cc
        src/display/bitwidth_histogram.cc
        src/io/resource.cc
        src/io/posting_offset_iterator.cc
        src/io/series_sample_iterator.cc
        src/query/filtered_index_iterator.cc

target_include_directories(plib PUBLIC src/)
target_link_libraries(plib PUBLIC Boost::boost Boost::program_options Boost::filesystem fmt::fmt)

add_executable(pdu
        src/pdu_main.cpp
        )

add_executable(pdump
        src/pdump_main.cpp
        )

target_link_libraries(pdu PUBLIC plib)
target_link_libraries(pdump PUBLIC plib)
