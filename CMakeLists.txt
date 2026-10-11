cmake_minimum_required(VERSION 3.20)

project(
    Fat32Tree
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(fat32tree-core
    src/core/BinaryReader.cpp
    src/boot/BootSectorParser.cpp
    src/boot/Fat32VolumeValidator.cpp
    src/fat/FatEntryParser.cpp
    src/fat/FatTableReader.cpp
    src/fat/ClusterChainReader.cpp
    src/directory/DirectoryEntryParser.cpp
    src/directory/DirectoryEntryValidator.cpp
    src/directory/DirectoryReader.cpp
    src/tree/FileTreeBuilder.cpp
    src/tree/TreePrinter.cpp
    src/core/Logger.cpp
)

target_include_directories(fat32tree-core
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

add_executable(fat32tree
    src/main.cpp
)

target_link_libraries(fat32tree
    PRIVATE
        fat32tree-core
)

enable_testing()

include(FetchContent)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

add_subdirectory(tests)
