#pragma once

#include "fat32tree/ClusterChainResult.h"

#include <cstdint>

namespace fat32tree
{

class Fat32VolumeLayout;
class FatTableReader;

class ClusterChainReader
{
public:
    ClusterChainReader(FatTableReader& fatTableReader, const Fat32VolumeLayout& layout);

    ClusterChainResult getClusterChain(std::uint32_t firstCluster) const;

private:
    bool isValidDataCluster(std::uint32_t clusterNumber) const;

    FatTableReader& m_fatTableReader;
    const Fat32VolumeLayout& m_layout;
};

} // namespace fat32tree