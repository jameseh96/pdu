
add_subdirectory(pdu)

add_executable(pdu
        pdu_main.cpp
        )

add_executable(pdump
        pdump_main.cpp
        )

target_link_libraries(pdu PUBLIC plib)
target_link_libraries(pdump PUBLIC plib)

add_subdirectory(pypdu)
