#pragma once

#include <cstdint>
#include <string>

namespace fat32tree
{

enum class DirectoryEntryType
{
    File,
    Directory
};

struct DirectoryEntry
{
    std::string name;
    DirectoryEntryType type;
    std::uint32_t firstCluster;
    std::uint32_t fileSize;
};

enum class DirectoryEntryState
{
    Normal,       // Valid short-name file or directory entry.
    Deleted,      // Entry is marked as deleted by DIR_Name[0] == 0xE5.
    LongFileName, // Long File Name entry (ATTR_LONG_NAME); ignored by this parser.
    VolumeLabel,  // Volume label entry describing the current FAT volume name.
    End,          // DIR_Name[0] == 0x00; no more directory entries follow.
    Invalid       // Malformed or unsupported short-name directory entry.
};

struct ParsedDirectoryEntry
{
    DirectoryEntryState state;
    DirectoryEntry entry;
};

} // namespace fat32tree