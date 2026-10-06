add_executable(fat32tree-tests
    SmokeTest.cpp
    BinaryReaderTest.cpp
)

target_link_libraries(fat32tree-tests
    PRIVATE
        fat32tree-core
        GTest::gtest_main
)

include(GoogleTest)

gtest_discover_tests(fat32tree-tests)