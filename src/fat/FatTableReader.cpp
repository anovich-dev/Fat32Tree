#include "fat32tree/fat/FatTableReader.h"

#include "fat32tree/boot/Fat32VolumeLayout.h"
#include "fat32tree/core/BinaryReader.h"
#include "fat32tree/core/Fat32Constants.h"

namespace fat32tree
{
    FatTableReader::FatTableReader(BinaryReader &reader, const Fat32VolumeLayout &layout)
        : m_reader(reader),
          m_layout(layout)
{
}

FatEntry FatTableReader::getEntry(std::uint32_t clusterNumber) const
{
    const std::uint64_t relativeOffset =
        static_cast<std::uint64_t>(clusterNumber) * fat32::kFatEntrySize;

    if (relativeOffset > m_layout.fatSizeBytes ||
        m_layout.fatSizeBytes - relativeOffset < fat32::kFatEntrySize)
    {
        throw FatTableReadException("FAT entry for cluster " + std::to_string(clusterNumber) +
                                    " is outside the selected FAT");
    }

    const std::uint64_t absoluteOffset = m_layout.selectedFatOffset + relativeOffset;

    const auto readResult = m_reader.readU32LE(absoluteOffset);

    if (readResult.status != ReadStatus::Success)
    {
        throw FatTableReadException(std::string("Failed to read FAT entry for cluster ") +
                                    std::to_string(clusterNumber) + ": " +
                                    readStatusToString(readResult.status));
    }

    return m_entryParser.parse(readResult.value);
}

} // namespace fat32tree
