find_package(Boost REQUIRED)
find_package(fmt REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(Snappy REQUIRED)
find_package(gsl-lite REQUIRED)


add_library(plib
        pdu/pdu.cc
        pdu/compute/compute.cc
        pdu/display/bitwidth_histogram.cc
        pdu/io/decoder.cc
        pdu/io/encoder.cc
        pdu/io/index.cc
        pdu/io/chunk_file_cache.cc
        pdu/io/chunk_reference.cc
        pdu/io/chunk_view.cc
        pdu/io/bit_decoder.cc
        pdu/io/head_chunks.cc
        pdu/io/mapped_file.cc
        pdu/io/index_iterator.cc
        pdu/io/resource.cc
        pdu/io/posting_offset_iterator.cc
        pdu/io/series_sample_iterator.cc
        pdu/io/wal.cc
        pdu/histogram/histogram.cc
        pdu/histogram/histogram_iterator.cc
        pdu/histogram/histogram_time_span.cc
        pdu/query/filtered_index_iterator.cc
        pdu/query/series_filter.cc
        pdu/query/sample_visitor.cc
        pdu/query/series_iterator.cc
        pdu/query/cross_index_sample_iterator.cc
        pdu/serialisation/deserialised_cross_index_series.cc
        pdu/serialisation/serialisation.cc
        pdu/util/host.cc)

set_property(TARGET plib PROPERTY CXX_VISIBILITY_PRESET hidden)

target_include_directories(plib PUBLIC .)
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
        pdu_main.cpp
        )

add_executable(pdump
        pdump_main.cpp
        )

target_link_libraries(pdu PUBLIC plib)
target_link_libraries(pdump PUBLIC plib)

add_subdirectory(pypdu)
