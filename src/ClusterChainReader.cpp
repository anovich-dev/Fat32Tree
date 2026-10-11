#include "fat32tree/ClusterChainReader.h"

#include "fat32tree/FatEntry.h"
#include "fat32tree/FatTableReader.h"
#include "fat32tree/Fat32VolumeLayout.h"
#include "fat32tree/Logger.h"

#include <unordered_set>

namespace fat32tree
{

ClusterChainReader::ClusterChainReader(FatTableReader &fatTableReader, const Fat32VolumeLayout &layout)
    : m_fatTableReader(fatTableReader),
      m_layout(layout)
{
}

 ClusterChainResult ClusterChainReader::getClusterChain(std::uint32_t firstCluster) const
{
    ClusterChainResult result;
    std::unordered_set<std::uint32_t> visited;

    std::uint32_t currentCluster = firstCluster;
    const auto warnIncompleteChain = [&result](const std::string& cause)
    {
        Logger::warning(cause + "; Cluster chain is incomplete; " +
            std::to_string(result.clusters.size()) +
            " cluster(s) were read before failure");
    };

    while (true)
    {
        if (!isValidDataCluster(currentCluster))
        {
            result.status = ClusterChainStatus::InvalidCluster;

            warnIncompleteChain("Invalid cluster encountered in chain: " +
                std::to_string(currentCluster));

            return result;
        }

        if (!visited.insert(currentCluster).second)
        {
            result.status = ClusterChainStatus::CycleDetected;

            warnIncompleteChain("Cluster chain cycle detected at cluster " +
                std::to_string(currentCluster));

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

            warnIncompleteChain("Failed to read FAT entry for cluster " +
                std::to_string(currentCluster));

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

                warnIncompleteChain("Free cluster encountered while traversing chain at cluster " +
                    std::to_string(currentCluster));

                break;

            case FatEntryType::Bad:
                result.status = ClusterChainStatus::BadCluster;

                warnIncompleteChain("Bad cluster encountered while traversing chain at cluster " +
                    std::to_string(currentCluster));

                break;

            case FatEntryType::Reserved:
                result.status = ClusterChainStatus::ReservedValue;

                warnIncompleteChain("Reserved FAT value encountered after cluster " +
                    std::to_string(currentCluster));

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