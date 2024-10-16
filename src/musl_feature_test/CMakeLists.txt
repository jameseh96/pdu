cmake_minimum_required(VERSION 3.17)
project(musl_feature_test)

set(CMAKE_CXX_STANDARD 17)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT error)

add_library(foo SHARED foo.cpp)
if( ipo_supported )
    message(STATUS "IPO / LTO enabled")
    set_property(TARGET foo PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()
target_compile_options(foo PRIVATE -O2 -flto)
target_link_options(foo PRIVATE -O2 -flto)
