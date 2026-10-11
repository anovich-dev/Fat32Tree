#pragma once

#include <cstdint>

#include "DirectoryEntry.h"

namespace fat32tree
{

class IDirectoryEntryParser
{
public:
    virtual ~IDirectoryEntryParser() = default;

    virtual ParsedDirectoryEntry parse(std::uint64_t entryOffset) const = 0;
};

} // namespace fat32tree