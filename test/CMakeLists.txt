find_package(GTest REQUIRED)

add_executable(pdu_test
        pdu_test.cpp
        )

target_include_directories(pdu_test PUBLIC ../src)
target_link_libraries(pdu_test PUBLIC
        plib
        GTest::gtest_main)