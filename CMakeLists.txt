cmake_minimum_required(VERSION 3.17)
project(prometheus_disk_usage)

set(CMAKE_CXX_STANDARD 17)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

include(CheckSymbolExists)
include(conan.cmake)

conan_cmake_configure(REQUIRES
        fmt/6.1.2
        boost/1.74.0
        nlohmann_json/3.9.1
        rapidjson/cci.20220822
        snappy/1.1.9
        gsl-lite/0.38.1
        gtest/1.11.0
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


# the pre-built fmt dep was compiled with _FORTIFY_SOURCE
# and requires __snprintf_chk, normally provided by glibc.
# If compiling for musl-based linux, e.g., Alpine, this is
# not available.
# In that case, use header-only fmt and accept the slower
# compilation.
check_symbol_exists(__snprintf_chk "" HAVE_SNPRINTF_CHK)

if (NOT HAVE_SNPRINTF_CHK)
    add_definitions(-DFMT_HEADER_ONLY)
endif ()


conan_cmake_install(PATH_OR_REFERENCE .
        BUILD missing
        REMOTE conancenter
        SETTINGS ${settings}
        PROFILE_HOST ${CONAN_PROFILE_HOST_ARG})

list(PREPEND CMAKE_MODULE_PATH "${CMAKE_BINARY_DIR}")

add_subdirectory(src)
add_subdirectory(test)
