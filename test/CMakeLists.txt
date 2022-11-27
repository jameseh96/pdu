find_package(GTest REQUIRED)

add_executable(pdu_test
        pdu_test.cpp
        )

target_include_directories(pdu_test PUBLIC ../src)
target_link_libraries(pdu_test PUBLIC
        plib
        GTest::gtest_main)

add_test(pdu_test ${CMAKE_CURRENT_BINARY_DIR}/pdu_test)
set_tests_properties(pdu_test PROPERTIES
        ENVIRONMENT
        DATADIR=${CMAKE_CURRENT_SOURCE_DIR}/data/)
