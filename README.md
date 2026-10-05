# Fat32Tree

Fat32Tree is a cross-platform C++20 console application for reading
and displaying the file tree of a FAT32 partition image.

The project is designed to run on macOS, Linux, and Windows and relies
on the C++ standard library for platform-independent filesystem image access.

The parser is designed to tolerate filesystem corruption where possible
and continue processing unaffected parts of the image.

## Requirements

- C++20-compatible compiler
- CMake 3.20+

GoogleTest is fetched automatically during CMake configuration.

## Build

cmake -S . -B build
cmake --build build

## Tests

ctest --test-dir build --output-on-failure