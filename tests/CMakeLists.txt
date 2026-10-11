add_executable(fat32tree-tests
    SmokeTest.cpp
    BinaryReaderTest.cpp
    BootSectorParserTest.cpp
    Fat32VolumeValidatorTest.cpp
    DirectoryEntryValidatorTest.cpp
    DirectoryEntryParserTest.cpp
    DirectoryReaderTest.cpp
    FileTreeBuilderTest.cpp
    TreePrinterTest.cpp
    integrationTests/Fat32InitializationTest.cpp
        integrationTests/Fat32ClusterChainIntegrationTest.cpp
    integrationTests/FileTreeBuilderIntegrationTest.cpp
    FatEntryParserTest.cpp
    FatTableReaderTest.cpp
    ClusterChainReaderTest.cpp
)

target_link_libraries(fat32tree-tests
    PRIVATE
        fat32tree-core
        GTest::gtest_main
        GTest::gmock
)

include(GoogleTest)

gtest_discover_tests(fat32tree-tests)
