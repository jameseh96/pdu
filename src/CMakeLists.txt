add_subdirectory(pdu)

add_executable(pdu
        exec/pdu_main.cpp
        exec/display/bitwidth_histogram.cc
        )

add_executable(pdump
        exec/pdump_main.cpp
        )

target_link_libraries(pdu PUBLIC plib)
target_link_libraries(pdump PUBLIC plib)

target_include_directories(pdu PUBLIC exec)

set_target_properties(pdu pdump
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
        )

add_subdirectory(pypdu)
