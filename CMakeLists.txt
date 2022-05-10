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


conan_cmake_install(PATH_OR_REFERENCE .
        BUILD missing
        REMOTE conancenter
        SETTINGS ${settings}
        PROFILE_HOST ${CONAN_PROFILE_HOST_ARG})

list(PREPEND CMAKE_MODULE_PATH "${CMAKE_BINARY_DIR}")

add_subdirectory(src)
add_subdirectory(test)
