#pragma once

#include "fat32tree/IDirectoryReader.h"

#include <gmock/gmock.h>

namespace fat32tree::test
{

class DirectoryReaderMock : public IDirectoryReader
{
public:
    MOCK_METHOD(DirectoryReadResult, readDirectory, (std::uint32_t firstCluster), (override));

    MOCK_METHOD(
        RootDirectoryReadResult, readRootDirectory, (std::uint32_t rootCluster), (override));
};

} // namespace fat32tree::test
