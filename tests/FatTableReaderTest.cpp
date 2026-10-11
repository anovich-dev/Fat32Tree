#include "fat32tree/BinaryReader.h"
#include "fat32tree/Fat32VolumeLayout.h"
#include "fat32tree/FatTableReader.h"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

#include <gtest/gtest.h>

namespace
{

class FatTableReaderTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        static std::atomic<std::uint64_t> nextId{0};
        const auto timestamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        const auto temporaryRoot = std::filesystem::temp_directory_path();

        for (int attempt = 0; attempt < 100; ++attempt)
        {
            const auto directoryName = "Fat32Tree-FatTableReader-" + std::to_string(timestamp) +
                                       "-" + std::to_string(nextId.fetch_add(1));
            const auto candidate = temporaryRoot / directoryName;
            std::error_code error;

            if (std::filesystem::create_directory(candidate, error))
            {
                m_temporaryDirectory = candidate;
                break;
            }

            ASSERT_FALSE(error) << error.message();
        }

        ASSERT_FALSE(m_temporaryDirectory.empty());
        m_imagePath = m_temporaryDirectory / "fat-image.bin";
    }

    void TearDown() override
    {
        std::error_code error;
        std::filesystem::remove_all(m_temporaryDirectory, error);
    }

    void writeImage(const std::vector<std::uint8_t>& bytes)
    {
        std::ofstream output(m_imagePath, std::ios::binary);
        ASSERT_TRUE(output.is_open());
        output.write(reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
        output.close();
        ASSERT_TRUE(output.good());
    }

    static void writeU32LE(
        std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value)
    {
        ASSERT_LE(offset + 4, bytes.size());
        bytes[offset] = static_cast<std::uint8_t>(value & 0xFFU);
        bytes[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFFU);
        bytes[offset + 2] = static_cast<std::uint8_t>((value >> 16) & 0xFFU);
        bytes[offset + 3] = static_cast<std::uint8_t>((value >> 24) & 0xFFU);
    }

    static fat32tree::Fat32VolumeLayout makeLayout()
    {
        fat32tree::Fat32VolumeLayout layout;
        layout.fatSizeBytes = 16;
        layout.fatOffset = 4;
        layout.selectedFatOffset = 16;
        return layout;
    }

protected:
    std::filesystem::path m_temporaryDirectory;
    std::filesystem::path m_imagePath;
};

TEST_F(FatTableReaderTest, ReadsNormalEntryAtSelectedFatOffset)
{
    std::vector<std::uint8_t> bytes(64, 0);
    writeU32LE(bytes, 16 + 2 * 4, 0x00000028);
    writeImage(bytes);

    fat32tree::BinaryReader reader(m_imagePath);
    const auto layout = makeLayout();
    const fat32tree::FatTableReader table(reader, layout);

    const auto entry = table.getEntry(2);

    EXPECT_EQ(entry.type, fat32tree::FatEntryType::NextCluster);
    EXPECT_EQ(entry.value, 40U);
}

TEST_F(FatTableReaderTest, MasksHighReservedNibbleFromFatEntry)
{
    std::vector<std::uint8_t> bytes(64, 0);
    writeU32LE(bytes, 16 + 2 * 4, 0xA0000028);
    writeImage(bytes);

    fat32tree::BinaryReader reader(m_imagePath);
    const auto layout = makeLayout();
    const fat32tree::FatTableReader table(reader, layout);

    const auto entry = table.getEntry(2);

    EXPECT_EQ(entry.type, fat32tree::FatEntryType::NextCluster);
    EXPECT_EQ(entry.value, 40U);
}

TEST_F(FatTableReaderTest, ReadsBadClusterEntry)
{
    std::vector<std::uint8_t> bytes(64, 0);
    writeU32LE(bytes, 16 + 2 * 4, 0x0FFFFFF7);
    writeImage(bytes);

    fat32tree::BinaryReader reader(m_imagePath);
    const auto layout = makeLayout();
    const fat32tree::FatTableReader table(reader, layout);

    const auto entry = table.getEntry(2);

    EXPECT_EQ(entry.type, fat32tree::FatEntryType::Bad);
    EXPECT_EQ(entry.value, 0x0FFFFFF7U);
}

TEST_F(FatTableReaderTest, ReadsEndOfChainEntry)
{
    std::vector<std::uint8_t> bytes(64, 0);
    writeU32LE(bytes, 16 + 2 * 4, 0x0FFFFFF8);
    writeImage(bytes);

    fat32tree::BinaryReader reader(m_imagePath);
    const auto layout = makeLayout();
    const fat32tree::FatTableReader table(reader, layout);

    const auto entry = table.getEntry(2);

    EXPECT_EQ(entry.type, fat32tree::FatEntryType::EndOfChain);
    EXPECT_EQ(entry.value, 0x0FFFFFF8U);
}

TEST_F(FatTableReaderTest, ReadsLastCompleteEntryInSelectedFat)
{
    std::vector<std::uint8_t> bytes(64, 0);
    writeU32LE(bytes, 16 + 12, 0x0000002A);
    writeImage(bytes);

    fat32tree::BinaryReader reader(m_imagePath);
    const auto layout = makeLayout();
    const fat32tree::FatTableReader table(reader, layout);

    const auto entry = table.getEntry(3);

    EXPECT_EQ(entry.type, fat32tree::FatEntryType::NextCluster);
    EXPECT_EQ(entry.value, 42U);
}

TEST_F(FatTableReaderTest, EntryPastFatEndThrows)
{
    std::vector<std::uint8_t> bytes(64, 0);
    writeImage(bytes);

    fat32tree::BinaryReader reader(m_imagePath);
    const auto layout = makeLayout();
    const fat32tree::FatTableReader table(reader, layout);

    try
    {
        (void)table.getEntry(4);
        FAIL() << "Expected FatTableReadException";
    }
    catch (const fat32tree::FatTableReadException& exception)
    {
        EXPECT_NE(
            std::string(exception.what()).find("outside the selected FAT"), std::string::npos);
    }
}

TEST_F(FatTableReaderTest, TruncatedImageThrowsWithReadStatus)
{
    // Cluster 2 is within the declared FAT range, but its bytes at offset 72
    // are beyond this image's 72-byte end.
    std::vector<std::uint8_t> bytes(72, 0);
    writeImage(bytes);

    fat32tree::BinaryReader reader(m_imagePath);
    auto layout = makeLayout();
    layout.selectedFatOffset = 64;
    const fat32tree::FatTableReader table(reader, layout);

    try
    {
        (void)table.getEntry(2);
        FAIL() << "Expected FatTableReadException";
    }
    catch (const fat32tree::FatTableReadException& exception)
    {
        EXPECT_NE(std::string(exception.what()).find("OutOfBounds"), std::string::npos);
    }
}

} // namespace
