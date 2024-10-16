cmake_minimum_required(VERSION 3.17)
project(musl_feature_test)

set(CMAKE_CXX_STANDARD 17)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_library(foo SHARED foo.cpp)
set_property(TARGET foo PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
target_compile_options(foo PRIVATE -O2 -flto)
target_link_options(foo PRIVATE -O2 -flto)
