#pragma once

#include <cstdint>

namespace fat32tree
{

struct Fat32Bpb
{
    std::uint16_t bytesPerSector = 0;
    std::uint8_t sectorsPerCluster = 0;
    std::uint16_t reservedSectorCount = 0;
    std::uint8_t fatCount = 0;

    std::uint16_t rootEntryCount = 0;
    std::uint16_t totalSectors16 = 0;
    std::uint16_t fatSize16 = 0;

    std::uint32_t totalSectors32 = 0;
    std::uint32_t fatSize32 = 0;

    std::uint16_t extendedFlags = 0;
    std::uint16_t fileSystemVersion = 0;

    std::uint32_t rootCluster = 0;

    std::uint16_t fsInfoSector = 0;
    std::uint16_t backupBootSector = 0;

    std::uint8_t signatureFirstByte = 0;
    std::uint8_t signatureSecondByte = 0;
};

} // namespace fat32tree