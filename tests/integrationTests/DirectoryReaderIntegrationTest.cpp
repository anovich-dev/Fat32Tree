#include "fat32tree/BinaryReader.h"
#include "fat32tree/ClusterChainReader.h"
#include "fat32tree/DirectoryEntryParser.h"
#include "fat32tree/DirectoryEntryValidator.h"
#include "fat32tree/DirectoryReader.h"
#include "fat32tree/Fat32Constants.h"
#include "fat32tree/Fat32VolumeLayout.h"
#include "fat32tree/FatTableReader.h"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <gtest/gtest.h>

namespace
{

namespace fat = fat32tree::fat32;

class DirectoryReaderIntegrationTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        static std::atomic<std::uint64_t> nextId{0};
        const auto timestamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        const auto temporaryRoot = std::filesystem::temp_directory_path();
        for (int attempt = 0; attempt < 100; ++attempt)
        {
            const auto candidate =
                temporaryRoot / ("Fat32Tree-DirectoryIntegration-" + std::to_string(timestamp) +
                                    "-" + std::to_string(nextId.fetch_add(1)));
            std::error_code error;
            if (std::filesystem::create_directory(candidate, error))
            {
                m_temporaryDirectory = candidate;
                break;
            }
            ASSERT_FALSE(error) << error.message();
        }
        ASSERT_FALSE(m_temporaryDirectory.empty());
        m_imagePath = m_temporaryDirectory / "directory.img";
    }

    void TearDown() override
    {
        if (!m_temporaryDirectory.empty())
        {
            std::error_code error;
            std::filesystem::remove_all(m_temporaryDirectory, error);
            EXPECT_FALSE(error) << error.message();
        }
    }

    void saveImage(const std::vector<std::uint8_t>& bytes)
    {
        std::ofstream image(m_imagePath, std::ios::binary);
        ASSERT_TRUE(image.is_open());
        image.write(reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
        image.close();
        ASSERT_TRUE(image.good());
    }

    static void writeLE(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value,
        std::size_t byteCount)
    {
        for (std::size_t i = 0; i < byteCount; ++i)
        {
            bytes.at(offset + i) = static_cast<std::uint8_t>(value >> (8 * i));
        }
    }

    static void writeFileEntry(std::vector<std::uint8_t>& bytes, std::size_t offset,
        std::string_view rawName, std::uint32_t cluster, std::uint32_t size)
    {
        ASSERT_EQ(rawName.size(), fat::kShortNameLength);
        for (std::size_t i = 0; i < rawName.size(); ++i)
        {
            bytes.at(offset + i) = static_cast<std::uint8_t>(rawName[i]);
        }
        bytes.at(offset + fat::kDirectoryAttributeOffset) = fat::kAttributeArchive;
        writeLE(bytes, offset + fat::kDirectoryFirstClusterHighOffset, cluster >> 16, 2);
        writeLE(bytes, offset + fat::kDirectoryFirstClusterLowOffset, cluster, 2);
        writeLE(bytes, offset + fat::kDirectoryFileSizeOffset, size, 4);
    }

protected:
    std::filesystem::path m_temporaryDirectory;
    std::filesystem::path m_imagePath;
};

TEST_F(DirectoryReaderIntegrationTest, SkipsInvalidDirectoryEntryAndContinuesReading)
{
    fat32tree::Fat32VolumeLayout layout{};
    layout.bytesPerSector = 512;
    layout.sectorsPerCluster = 1;
    layout.clusterSize = 512;
    layout.fatCount = 1;
    layout.fatSizeSectors = 1;
    layout.fatSizeBytes = 512;
    layout.fatOffset = 512;
    layout.selectedFatOffset = 512;
    layout.firstDataSector = 2;
    layout.dataOffset = 1024;
    layout.clusterCount = 8;
    layout.dataSectors = 8;
    layout.totalSectors = 10;
    layout.volumeSize = 10 * 512;
    layout.rootCluster = 2;

    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(layout.volumeSize), 0);
    for (const auto cluster : {2U, 3U, 4U, 5U})
    {
        writeLE(
            bytes, layout.selectedFatOffset + cluster * fat::kFatEntrySize, fat::kEndOfChainMax, 4);
    }
    const auto directoryOffset = static_cast<std::size_t>(layout.dataOffset);
    writeFileEntry(bytes, directoryOffset, "FILE1   TXT", 3, 111);
    // '?' is forbidden by the real short-name validator. All 32 bytes are
    // readable, and the slot is neither deleted, LFN, nor an End marker.
    const auto invalidOffset = directoryOffset + fat::kDirectoryEntrySize;
    writeFileEntry(bytes, invalidOffset, "BAD?    TXT", 4, 999);
    writeFileEntry(bytes, directoryOffset + 2 * fat::kDirectoryEntrySize, "FILE2   TXT", 5, 222);

    // The zero-filled fourth slot supplies the End marker.
    ASSERT_NO_FATAL_FAILURE(saveImage(bytes));

    fat32tree::BinaryReader reader(m_imagePath);
    fat32tree::DirectoryEntryParser parser(
        reader, std::make_unique<fat32tree::DirectoryEntryValidator>());
    ASSERT_EQ(parser.parse(invalidOffset).state, fat32tree::DirectoryEntryState::Invalid);
    ASSERT_EQ(parser.parse(directoryOffset + 3 * fat::kDirectoryEntrySize).state,
        fat32tree::DirectoryEntryState::End);

    fat32tree::FatTableReader fatTableReader(reader, layout);
    auto directoryReader = std::make_unique<fat32tree::DirectoryReader>(layout,
        std::make_unique<fat32tree::ClusterChainReader>(fatTableReader, layout),
        std::make_unique<fat32tree::DirectoryEntryParser>(
            reader, std::make_unique<fat32tree::DirectoryEntryValidator>()));

    const auto result = directoryReader->readDirectory(layout.rootCluster);

    EXPECT_EQ(result.chainStatus, fat32tree::ClusterChainStatus::Complete);
    ASSERT_EQ(result.entries.size(), 2U);
    EXPECT_EQ(result.entries[0].name, "FILE1.TXT");
    EXPECT_EQ(result.entries[0].type, fat32tree::DirectoryEntryType::File);
    EXPECT_EQ(result.entries[0].firstCluster, 3U);
    EXPECT_EQ(result.entries[0].fileSize, 111U);
    EXPECT_EQ(result.entries[1].name, "FILE2.TXT");
    EXPECT_EQ(result.entries[1].type, fat32tree::DirectoryEntryType::File);
    EXPECT_EQ(result.entries[1].firstCluster, 5U);
    EXPECT_EQ(result.entries[1].fileSize, 222U);
}

TEST_F(DirectoryReaderIntegrationTest, StopsDirectoryScanAtEndMarkerAndIgnoresFollowingEntries)
{
    constexpr std::uint32_t kFile1FirstCluster = 3;
    constexpr std::uint32_t kFile2FirstCluster = 4;
    constexpr std::uint32_t kFile1SizeBytes = 111;
    constexpr std::uint32_t kFile2SizeBytes = 222;

    fat32tree::Fat32VolumeLayout layout{};
    layout.bytesPerSector = 512;
    layout.sectorsPerCluster = 1;
    layout.clusterSize = 512;
    layout.fatCount = 1;
    layout.fatSizeSectors = 1;
    layout.fatSizeBytes = 512;
    layout.fatOffset = 512;
    layout.selectedFatOffset = 512;
    layout.firstDataSector = 2;
    layout.dataOffset = 1024;
    layout.clusterCount = 8;
    layout.dataSectors = 8;
    layout.totalSectors = 10;
    layout.volumeSize = 10 * 512;
    layout.rootCluster = 2;

    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(layout.volumeSize), 0);
    for (const auto cluster : {layout.rootCluster, kFile1FirstCluster, kFile2FirstCluster})
    {
        writeLE(
            bytes, layout.selectedFatOffset + cluster * fat::kFatEntrySize, fat::kEndOfChainMax, 4);
    }
    const auto directoryOffset = static_cast<std::size_t>(layout.dataOffset);
    const auto endOffset = directoryOffset + fat::kDirectoryEntrySize;
    const auto followingEntryOffset = directoryOffset + 2 * fat::kDirectoryEntrySize;
    writeFileEntry(bytes, directoryOffset, "FILE1   TXT", kFile1FirstCluster, kFile1SizeBytes);
    bytes.at(endOffset) = fat::kDirectoryEntryEndMarker;
    writeFileEntry(bytes, followingEntryOffset, "FILE2   TXT", kFile2FirstCluster, kFile2SizeBytes);
    ASSERT_NO_FATAL_FAILURE(saveImage(bytes));

    fat32tree::BinaryReader reader(m_imagePath);
    fat32tree::DirectoryEntryParser parser(
        reader, std::make_unique<fat32tree::DirectoryEntryValidator>());
    ASSERT_EQ(parser.parse(endOffset).state, fat32tree::DirectoryEntryState::End);

    // Prove that FILE2 is a valid entry which would be included if the scan
    // incorrectly continued past the End marker.
    const auto followingEntry = parser.parse(followingEntryOffset);
    ASSERT_EQ(followingEntry.state, fat32tree::DirectoryEntryState::Normal);
    ASSERT_EQ(followingEntry.entry.name, "FILE2.TXT");

    fat32tree::FatTableReader fatTableReader(reader, layout);
    fat32tree::DirectoryReader directoryReader(layout,
        std::make_unique<fat32tree::ClusterChainReader>(fatTableReader, layout),
        std::make_unique<fat32tree::DirectoryEntryParser>(
            reader, std::make_unique<fat32tree::DirectoryEntryValidator>()));

    const auto result = directoryReader.readDirectory(layout.rootCluster);

    EXPECT_EQ(result.chainStatus, fat32tree::ClusterChainStatus::Complete);
    ASSERT_EQ(result.entries.size(), 1U);
    EXPECT_EQ(result.entries[0].name, "FILE1.TXT");
    EXPECT_EQ(result.entries[0].type, fat32tree::DirectoryEntryType::File);
    EXPECT_EQ(result.entries[0].firstCluster, kFile1FirstCluster);
    EXPECT_EQ(result.entries[0].fileSize, kFile1SizeBytes);
}

TEST_F(DirectoryReaderIntegrationTest, PreservesReadableEntriesWhenDirectoryDataIsTruncated)
{
    constexpr std::uint32_t kFile1FirstCluster = 3;
    constexpr std::uint32_t kFile2FirstCluster = 4;
    constexpr std::uint32_t kFile1SizeBytes = 111;
    constexpr std::uint32_t kFile2SizeBytes = 222;

    fat32tree::Fat32VolumeLayout layout{};
    layout.bytesPerSector = 512;
    layout.sectorsPerCluster = 8;
    layout.clusterSize = 4096;
    layout.fatCount = 1;
    layout.fatSizeSectors = 1;
    layout.fatSizeBytes = 512;
    layout.fatOffset = 512;
    layout.selectedFatOffset = 512;
    layout.firstDataSector = 2;
    layout.dataOffset = 1024;
    layout.clusterCount = 8;
    layout.dataSectors = 64;
    layout.totalSectors = 66;
    layout.volumeSize = 66 * 512;
    layout.rootCluster = 2;

    const auto directoryOffset = static_cast<std::size_t>(layout.dataOffset);
    const auto truncatedOffset = directoryOffset + 2 * fat::kDirectoryEntrySize;
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(layout.volumeSize), 0);
    for (const auto cluster : {layout.rootCluster, kFile1FirstCluster, kFile2FirstCluster})
    {
        writeLE(
            bytes, layout.selectedFatOffset + cluster * fat::kFatEntrySize, fat::kEndOfChainMax, 4);
    }
    writeFileEntry(bytes, directoryOffset, "FILE1   TXT", kFile1FirstCluster, kFile1SizeBytes);
    writeFileEntry(bytes, directoryOffset + fat::kDirectoryEntrySize, "FILE2   TXT",
        kFile2FirstCluster, kFile2SizeBytes);
    writeFileEntry(bytes, truncatedOffset, "FILE3   TXT", 5, 333);
    // Preserve the third slot's name and attributes, but truncate its cluster
    // and size fields. There is no End marker before EOF.
    bytes.resize(truncatedOffset + fat::kDirectoryFirstClusterHighOffset + 1);
    ASSERT_NO_FATAL_FAILURE(saveImage(bytes));

    fat32tree::BinaryReader reader(m_imagePath);
    ASSERT_EQ(reader.size(), bytes.size());
    ASSERT_LT(reader.size(), layout.dataOffset + layout.clusterSize);
    ASSERT_EQ(reader.readU16LE(truncatedOffset + fat::kDirectoryFirstClusterHighOffset).status,
        fat32tree::ReadStatus::OutOfBounds);

    fat32tree::DirectoryEntryParser parser(
        reader, std::make_unique<fat32tree::DirectoryEntryValidator>());
    ASSERT_EQ(parser.parse(truncatedOffset).state, fat32tree::DirectoryEntryState::Invalid);
    ASSERT_EQ(parser.parse(truncatedOffset + fat::kDirectoryEntrySize).state,
        fat32tree::DirectoryEntryState::Invalid);

    fat32tree::FatTableReader fatTableReader(reader, layout);
    fat32tree::DirectoryReader directoryReader(layout,
        std::make_unique<fat32tree::ClusterChainReader>(fatTableReader, layout),
        std::make_unique<fat32tree::DirectoryEntryParser>(
            reader, std::make_unique<fat32tree::DirectoryEntryValidator>()));

    // Physical read failures currently become Invalid, so scanning continues
    // through the remaining slots, with a warning for each unreadable slot.
    // Work is bounded by chain length * entries per cluster. This test checks
    // preservation of readable entries without requiring that warning count.
    const auto result = directoryReader.readDirectory(layout.rootCluster);

    // chainStatus describes FAT traversal, whose bytes remain fully readable.
    EXPECT_EQ(result.chainStatus, fat32tree::ClusterChainStatus::Complete);
    ASSERT_EQ(result.entries.size(), 2U);
    EXPECT_EQ(result.entries[0].name, "FILE1.TXT");
    EXPECT_EQ(result.entries[0].type, fat32tree::DirectoryEntryType::File);
    EXPECT_EQ(result.entries[0].firstCluster, kFile1FirstCluster);
    EXPECT_EQ(result.entries[0].fileSize, kFile1SizeBytes);
    EXPECT_EQ(result.entries[1].name, "FILE2.TXT");
    EXPECT_EQ(result.entries[1].type, fat32tree::DirectoryEntryType::File);
    EXPECT_EQ(result.entries[1].firstCluster, kFile2FirstCluster);
    EXPECT_EQ(result.entries[1].fileSize, kFile2SizeBytes);
}

} // namespace
