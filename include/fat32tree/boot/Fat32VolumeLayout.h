#pragma once

#include <cstdint>

namespace fat32tree
{

struct Fat32VolumeLayout
{
    // Sector and cluster geometry
    std::uint64_t bytesPerSector = 0;
    std::uint64_t sectorsPerCluster = 0;
    std::uint64_t clusterSize = 0;

    // Volume geometry
    std::uint64_t totalSectors = 0;
    std::uint64_t volumeSize = 0;

    // FAT geometry
    std::uint64_t fatCount = 0;
    std::uint64_t fatSizeSectors = 0;
    std::uint64_t fatSizeBytes = 0;
    std::uint64_t fatOffset = 0;
    std::uint32_t selectedFatIndex = 0;
    std::uint64_t selectedFatOffset = 0;

    // Data region geometry
    std::uint64_t firstDataSector = 0;
    std::uint64_t dataOffset = 0;
    std::uint64_t dataSectors = 0;
    std::uint64_t clusterCount = 0;

    // Root directory
    std::uint32_t rootCluster = 0;
};

} // namespace fat32tree
