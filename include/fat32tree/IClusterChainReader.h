#pragma once

#include "fat32tree/ClusterChainResult.h"

#include <cstdint>

namespace fat32tree
{

class IClusterChainReader
{
public:
    virtual ~IClusterChainReader() = default;

    virtual ClusterChainResult getClusterChain(std::uint32_t firstCluster) const = 0;
};

} // namespace fat32tree
