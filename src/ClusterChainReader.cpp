#include "fat32tree/ClusterChainReader.h"

#include "fat32tree/FatEntry.h"
#include "fat32tree/FatTableReader.h"

#include <unordered_set>

namespace fat32tree
{

    ClusterChainReader::ClusterChainReader(FatTableReader &fatTableReader, const Fat32VolumeLayout &layout)
        : m_fatTableReader(fatTableReader),
          m_layout(layout)
    {
    }

 ClusterChainResult ClusterChainReader::getClusterChain(
    std::uint32_t firstCluster) const
{
    ClusterChainResult result;
    std::unordered_set<std::uint32_t> visited;

    std::uint32_t currentCluster = firstCluster;

    while (true)
    {
        if (!isValidDataCluster(currentCluster))
        {
            result.status = ClusterChainStatus::InvalidCluster;
            return result;
        }

        if (!visited.insert(currentCluster).second)
        {
            result.status = ClusterChainStatus::CycleDetected;
            return result;
        }

        result.clusters.push_back(currentCluster);

        FatEntry entry;

        try
        {
            entry = m_fatTableReader.getEntry(currentCluster);
        }
        catch (const FatTableReadException&)
        {
            result.status = ClusterChainStatus::ReadError;
            return result;
        }

        switch (entry.type)
        {
            case FatEntryType::NextCluster:
                currentCluster = entry.value;
                continue;

            case FatEntryType::EndOfChain:
                result.status = ClusterChainStatus::Complete;
                break;

            case FatEntryType::Free:
                result.status = ClusterChainStatus::FreeCluster;
                break;

            case FatEntryType::Bad:
                result.status = ClusterChainStatus::BadCluster;
                break;

            case FatEntryType::Reserved:
                result.status = ClusterChainStatus::ReservedValue;
                break;
        }

        return result;
    }
}
    bool ClusterChainReader::isValidDataCluster(std::uint32_t clusterNumber) const
    {
        if (clusterNumber < 2)
        {
            return false;
        }

        return static_cast<std::uint64_t>(clusterNumber - 2) < m_layout.clusterCount;
    }

}