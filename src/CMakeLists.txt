add_subdirectory(pdu)

add_executable(pdu
        pdu_main.cpp
        )

add_executable(pdump
        pdump_main.cpp
        )

target_link_libraries(pdu PUBLIC plib)
target_link_libraries(pdump PUBLIC plib)

set_target_properties(pdu pdump
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
        )

add_subdirectory(pypdu)
