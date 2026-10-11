#pragma once

#include "fat32tree/directory/DirectoryEntry.h"
#include "fat32tree/fat/ClusterChainResult.h"

#include <optional>
#include <string>
#include <vector>

namespace fat32tree
{

struct DirectoryReadResult
{
    std::vector<DirectoryEntry> entries;
    ClusterChainStatus chainStatus;
};

struct RootDirectoryReadResult
{
    std::vector<DirectoryEntry> entries;
    std::optional<std::string> volumeLabel;
    ClusterChainStatus chainStatus;
};

} // namespace fat32tree
