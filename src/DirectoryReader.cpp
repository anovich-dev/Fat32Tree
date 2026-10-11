#include "fat32tree/DirectoryReader.h"

#include "fat32tree/Fat32Constants.h"
#include "fat32tree/Fat32VolumeLayout.h"
#include "fat32tree/Logger.h"

#include <utility>

namespace fat32tree
{

DirectoryReader::DirectoryReader(const Fat32VolumeLayout& layout,
    std::unique_ptr<IClusterChainReader> chainReader,
    std::unique_ptr<IDirectoryEntryParser> entryParser)
    : m_layout(layout), m_chainReader(std::move(chainReader)),
      m_entryParser(std::move(entryParser)),
      m_entriesPerCluster(layout.clusterSize / fat32::kDirectoryEntrySize)
{
}

DirectoryReadResult DirectoryReader::readDirectory(std::uint32_t firstCluster)
{
    auto scanResult = scanDirectory(firstCluster);

    return {
        std::move(scanResult.entries),
        scanResult.chainStatus
    };
}

RootDirectoryReadResult DirectoryReader::readRootDirectory(std::uint32_t rootCluster)
{
    auto scanResult = scanDirectory(rootCluster);

    return {
        std::move(scanResult.entries),
        std::move(scanResult.volumeLabel),
        scanResult.chainStatus
    };
}

/* Uses RootDirectoryReadResult as the common scan result because it contains
   all data discovered during directory traversal, including an optional volume label.
   readDirectory() simply discards the label for non-root directories. */

RootDirectoryReadResult DirectoryReader::scanDirectory(std::uint32_t firstCluster)
{
    RootDirectoryReadResult result{};

    const auto chainResult = m_chainReader->getClusterChain(firstCluster);

    result.chainStatus = chainResult.status;
    if (chainResult.status != ClusterChainStatus::Complete)
    {
        Logger::warning("Directory starting at cluster " + std::to_string(firstCluster) +
                        " has an incomplete cluster chain");
    }

    for (const std::uint32_t cluster : chainResult.clusters)
    {
        const std::uint64_t clusterOffset = clusterToOffset(cluster);

        for (std::uint64_t entryIndex = 0; entryIndex < m_entriesPerCluster; ++entryIndex)
        {
            const std::uint64_t entryOffset =
                clusterOffset + entryIndex * fat32::kDirectoryEntrySize;

            const ParsedDirectoryEntry parsed = m_entryParser->parse(entryOffset);

            switch (parsed.state)
            {
            case DirectoryEntryState::Normal:
            {
                    if (parsed.entry.name == "." ||
                        parsed.entry.name == "..")
                {
                    continue;
                }

                result.entries.push_back(parsed.entry);

                continue;
            }

            case DirectoryEntryState::VolumeLabel:
            {
                if (!result.volumeLabel.has_value())
                {
                    result.volumeLabel = parsed.entry.name;
                }

                continue;
            }

            case DirectoryEntryState::Deleted:
            case DirectoryEntryState::LongFileName:
                continue;

            case DirectoryEntryState::Invalid:
                Logger::warning(
                    "Invalid directory entry skipped at offset " + std::to_string(entryOffset));
                
                    continue;

            case DirectoryEntryState::End:
                return result;
            }
        }
    }

    return result;
}

std::uint64_t DirectoryReader::clusterToOffset(std::uint32_t cluster) const
{
    return m_layout.dataOffset + static_cast<std::uint64_t>(cluster - 2) * m_layout.clusterSize;
}

} // namespace fat32tree
