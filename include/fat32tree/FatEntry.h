#pragma once

#include <cstdint>

namespace fat32tree
{

enum class FatEntryType
{
    Free,        // 0x00000000
    NextCluster, // 0x00000002..MAX for the current volume
    Reserved,    // reserved FAT32 value
    Bad,         // 0x0FFFFFF7
    EndOfChain   // 0x0FFFFFF8..0x0FFFFFFF
};

struct FatEntry
{
    FatEntryType type;
    std::uint32_t value;
};

} // namespace fat32tree
