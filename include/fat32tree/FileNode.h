#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>

#include "fat32tree/DirectoryEntry.h"

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