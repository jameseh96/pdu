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

add_executable(pdu
        src/main.cpp
        src/decoder.cc
        src/index.cc
        src/chunk_file_cache.cc
        src/bit_decoder.cc)

target_link_libraries(pdu PUBLIC Boost::boost Boost::program_options Boost::filesystem fmt::fmt)
