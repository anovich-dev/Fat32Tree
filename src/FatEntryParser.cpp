#include "fat32tree/FatEntryParser.h"
#include "fat32tree/Fat32Constants.h"

namespace fat32tree
{
    
FatEntry FatEntryParser::parse(std::uint32_t rawValue) const
{
    const std::uint32_t value = rawValue & fat32::kFatEntryValueMask;

    if (value == fat32::kFreeClusterValue)
    {
        return {FatEntryType::Free, value};
    }

    if (value == fat32::kReservedClusterValue ||
    (value >= fat32::kReservedClusterMin &&
     value <= fat32::kReservedClusterMax))
    {
        return {FatEntryType::Reserved, value};
    }

    if (value == fat32::kBadClusterValue)
    {
        return {FatEntryType::Bad, value};
    }

    if (value >= fat32::kEndOfChainMin)
    {
        return {FatEntryType::EndOfChain, value};
    }

    return {FatEntryType::NextCluster, value};
}

} // namespace fat32tree