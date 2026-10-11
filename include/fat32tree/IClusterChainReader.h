#pragma once

#include <cstdint>

#include "ClusterChainResult.h"

namespace fat32tree
{

class IClusterChainReader
{
public:
    virtual ~IClusterChainReader() = default;

    virtual ClusterChainResult getClusterChain(std::uint32_t firstCluster) const = 0;
};

} // namespace fat32tree