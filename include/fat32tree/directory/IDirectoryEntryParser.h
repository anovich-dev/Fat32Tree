#pragma once

#include "fat32tree/directory/DirectoryEntry.h"

#include <cstdint>

namespace fat32tree
{

class IDirectoryEntryParser
{
public:
    virtual ~IDirectoryEntryParser() = default;

    virtual ParsedDirectoryEntry parse(std::uint64_t entryOffset) const = 0;
};

} // namespace fat32tree
