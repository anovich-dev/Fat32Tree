#pragma once

#include <vector>

namespace fat32tree
{

enum class ClusterChainStatus
{
    Complete,       // Chain ended normally with an End-Of-Chain marker.
    FreeCluster,    // Chain unexpectedly reached a FAT entry marked as free.
    BadCluster,     // Chain encountered the FAT32 bad-cluster marker.
    ReservedValue,  // Chain encountered a reserved FAT32 entry value.
    InvalidCluster, // Cluster number is outside the valid range of this volume.
    CycleDetected,  // A cluster was encountered more than once in the same chain.
    ReadError      // FAT entry could not be read from the image.
};

struct ClusterChainResult
{
    std::vector<std::uint32_t> clusters;
    ClusterChainStatus status = ClusterChainStatus::Complete;
};

} // namespace fat32tree