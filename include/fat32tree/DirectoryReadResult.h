#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "ClusterChainResult.h"
#include "DirectoryEntry.h"

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