cmake_minimum_required(VERSION 3.17)
if(NOT CMAKE_TOOLCHAIN_FILE)
if (NOT DEFINED "$ENV{VCPKG_ROOT}")
    message(WARNING "VCPKG_ROOT env var not found, install vcpkg and set the env var")
endif()
set(vcpkg "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
if (NOT EXISTS "${vcpkg}")
set(vcpkg "$ENV{HOME}/vcpkg/scripts/buildsystems/vcpkg.cmake")
endif()
if (EXISTS "${vcpkg}")
  set(CMAKE_TOOLCHAIN_FILE "${vcpkg}"
      CACHE FILEPATH "CMake toolchain file")
  message(STATUS "vcpkg toolchain found: ${CMAKE_TOOLCHAIN_FILE}")
else()
message(STATUS "vcpkg toolchain NOT found")
endif()
endif()
set(VCPKG_OVERLAY_PORTS "${CMAKE_SOURCE_DIR}/ports/overlay")

project(prometheus_disk_usage)

set(CMAKE_CXX_STANDARD 17)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

include(CheckSymbolExists)
INCLUDE(CheckCXXSourceCompiles)
cmake_policy(SET CMP0067 NEW)

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

try_compile(IPO_FORTIFY_SOURCE_WORKING PROJECT musl_feature_test
            SOURCE_DIR "${CMAKE_SOURCE_DIR}/src/musl_feature_test/"
            TARGET foo)
if (NOT IPO_FORTIFY_SOURCE_WORKING)
        message(WARNING "-D_FORTIFY_SOURCE will cause link time failures due to ipo/lto and vsnprintf, disabling")
        add_definitions(-U_FORTIFY_SOURCE)
endif()

list(PREPEND CMAKE_MODULE_PATH "${CMAKE_BINARY_DIR}")

add_subdirectory(src)
add_subdirectory(test)
