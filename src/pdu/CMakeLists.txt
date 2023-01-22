find_package(Boost REQUIRED)
find_package(fmt REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(Snappy REQUIRED)
find_package(gsl-lite REQUIRED)


add_library(plib
        pdu.cc
        expression/expression.cc
        encode/bit_decoder.cc
        encode/bit_encoder.cc
        encode/decoder.cc
        encode/encoder.cc
        block/index.cc
        block/chunk_builder.cc
        block/chunk_file_cache.cc
        block/chunk_iterator.cc
        block/chunk_reference.cc
        block/chunk_view.cc
        block/chunk_writer.cc
        block/head_chunks.cc
        block/mapped_file.cc
        block/index_iterator.cc
        block/resource.cc
        block/posting_offset_iterator.cc
        block/sample.cc
        block/series_sample_iterator.cc
        block/wal.cc
        histogram/histogram.cc
        histogram/histogram_iterator.cc
        histogram/histogram_time_span.cc
        filter/filtered_index_iterator.cc
        filter/series_filter.cc
        filter/sample_visitor.cc
        filter/series_iterator.cc
        filter/cross_index_sample_iterator.cc
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