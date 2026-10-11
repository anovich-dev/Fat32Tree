#pragma once

#include "fat32tree/DirectoryEntry.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace fat32tree
{

struct FileNode
{
    std::string name;
    DirectoryEntryType type;
    std::uint32_t firstCluster;
    std::uint32_t fileSize;
    std::vector<std::unique_ptr<FileNode>> children;
};

} // namespace fat32tree
