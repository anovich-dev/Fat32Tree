#pragma once

#include "fat32tree/fat/ClusterChainResult.h"
#include "fat32tree/fat/IClusterChainReader.h"

#include <cstdint>

namespace fat32tree
{

struct Fat32VolumeLayout;
class FatTableReader;

class ClusterChainReader final : public IClusterChainReader
{
public:
    ClusterChainReader(FatTableReader& fatTableReader, const Fat32VolumeLayout& layout);

    ClusterChainResult getClusterChain(std::uint32_t firstCluster) const override;

private:
    bool isValidDataCluster(std::uint32_t clusterNumber) const;

private:
    FatTableReader& m_fatTableReader;
    const Fat32VolumeLayout& m_layout;
};

} // namespace fat32tree
