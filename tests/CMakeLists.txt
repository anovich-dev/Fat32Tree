add_executable(fat32tree-tests
    SmokeTest.cpp
)

target_link_libraries(fat32tree-tests
    PRIVATE
        GTest::gtest_main
)

include(GoogleTest)

gtest_discover_tests(fat32tree-tests)