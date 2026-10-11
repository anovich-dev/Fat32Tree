#pragma once

#include "fat32tree/DirectoryReadResult.h"

#include <cstdint>

namespace fat32tree
{

class IDirectoryReader
{
public:
    virtual ~IDirectoryReader() = default;

    virtual DirectoryReadResult readDirectory(std::uint32_t firstCluster) = 0;

    virtual RootDirectoryReadResult readRootDirectory(std::uint32_t rootCluster) = 0;
};

} // namespace fat32tree
