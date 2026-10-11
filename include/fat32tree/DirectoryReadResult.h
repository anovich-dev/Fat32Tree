#pragma once

#include "fat32tree/ClusterChainResult.h"
#include "fat32tree/DirectoryEntry.h"

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
