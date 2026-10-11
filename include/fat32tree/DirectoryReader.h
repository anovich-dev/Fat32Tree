#pragma once

#include "IDirectoryReader.h"

#include "DirectoryReadResult.h"
#include "Fat32VolumeLayout.h"

#include <cstdint>
#include <memory>

namespace fat32tree
{

class IClusterChainReader;
class IDirectoryEntryParser;

class DirectoryReader final : public IDirectoryReader
{
public:
    DirectoryReader(
        const Fat32VolumeLayout& layout,
        std::unique_ptr<IClusterChainReader> chainReader,
        std::unique_ptr<IDirectoryEntryParser> entryParser);

    DirectoryReadResult readDirectory(std::uint32_t firstCluster) override;

    RootDirectoryReadResult readRootDirectory(std::uint32_t rootCluster) override;

private:
    std::uint64_t clusterToOffset(std::uint32_t cluster) const;

    RootDirectoryReadResult scanDirectory(std::uint32_t firstCluster);

private:
    const Fat32VolumeLayout& m_layout;

    std::unique_ptr<IClusterChainReader> m_chainReader;
    std::unique_ptr<IDirectoryEntryParser> m_entryParser;

    std::uint64_t m_entriesPerCluster;
};

} // namespace fat32tree