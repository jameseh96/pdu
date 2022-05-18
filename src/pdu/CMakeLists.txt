find_package(Boost REQUIRED)
find_package(fmt REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(Snappy REQUIRED)
find_package(gsl-lite REQUIRED)


add_library(plib
        pdu.cc
        expression/expression.cc
        display/bitwidth_histogram.cc
        io/decoder.cc
        io/encoder.cc
        io/index.cc
        io/chunk_file_cache.cc
        io/chunk_reference.cc
        io/chunk_view.cc
        io/bit_decoder.cc
        io/head_chunks.cc
        io/mapped_file.cc
        io/index_iterator.cc
        io/resource.cc
        io/posting_offset_iterator.cc
        io/series_sample_iterator.cc
        io/wal.cc
        histogram/histogram.cc
        histogram/histogram_iterator.cc
        histogram/histogram_time_span.cc
        query/filtered_index_iterator.cc
        query/series_filter.cc
        query/sample_visitor.cc
        query/series_iterator.cc
        query/cross_index_sample_iterator.cc
        serialisation/deserialised_cross_index_series.cc
        serialisation/serialisation.cc
        util/host.cc)

set_property(TARGET plib PROPERTY CXX_VISIBILITY_PRESET hidden)

target_include_directories(plib PUBLIC ..)
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