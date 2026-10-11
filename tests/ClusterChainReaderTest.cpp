#include "fat32tree/boot/Fat32VolumeLayout.h"
#include "fat32tree/core/BinaryReader.h"
#include "fat32tree/fat/ClusterChainReader.h"
#include "fat32tree/fat/FatTableReader.h"

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

class ClusterChainReaderTest : public ::testing::Test
{
protected:
    static constexpr std::uint64_t kSelectedFatOffset = 16;
    static constexpr std::uint64_t kFatSizeBytes = 512;
    static constexpr std::uint64_t kClusterCount = 100;

    void SetUp() override
    {
        static std::atomic<std::uint64_t> nextId{0};
        const auto timestamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        const auto temporaryRoot = std::filesystem::temp_directory_path();

        for (int attempt = 0; attempt < 100; ++attempt)
        {
            const auto directoryName = "Fat32Tree-ClusterChainReader-" + std::to_string(timestamp) +
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

    static std::size_t fatEntryOffset(std::uint32_t clusterNumber)
    {
        return static_cast<std::size_t>(
            kSelectedFatOffset + static_cast<std::uint64_t>(clusterNumber) * 4);
    }

    static std::vector<std::uint8_t> fatImage()
    {
        return std::vector<std::uint8_t>(
            static_cast<std::size_t>(kSelectedFatOffset + kFatSizeBytes), 0);
    }

    static fat32tree::Fat32VolumeLayout makeLayout()
    {
        fat32tree::Fat32VolumeLayout layout;
        layout.fatSizeBytes = kFatSizeBytes;
        layout.selectedFatOffset = kSelectedFatOffset;
        layout.clusterCount = kClusterCount;
        return layout;
    }

protected:
    std::filesystem::path m_temporaryDirectory;
    std::filesystem::path m_imagePath;
};

TEST_F(ClusterChainReaderTest, SingleClusterEndsNormally)
{
    auto bytes = fatImage();
    writeU32LE(bytes, fatEntryOffset(20), 0x0FFFFFF8);
    writeImage(bytes);

    fat32tree::BinaryReader reader(m_imagePath);
    const auto layout = makeLayout();
    fat32tree::FatTableReader fatTable(reader, layout);
    const fat32tree::ClusterChainReader chainReader(fatTable, layout);

    const auto result = chainReader.getClusterChain(20);

    EXPECT_EQ(result.status, fat32tree::ClusterChainStatus::Complete);
    EXPECT_EQ(result.clusters, (std::vector<std::uint32_t>{20}));
}

TEST_F(ClusterChainReaderTest, ReadsFragmentedChainThroughEndOfChain)
{
    auto bytes = fatImage();
    writeU32LE(bytes, fatEntryOffset(20), 21);
    writeU32LE(bytes, fatEntryOffset(21), 40);
    writeU32LE(bytes, fatEntryOffset(40), 0x0FFFFFF8);
    writeImage(bytes);

    fat32tree::BinaryReader reader(m_imagePath);
    const auto layout = makeLayout();
    fat32tree::FatTableReader fatTable(reader, layout);
    const fat32tree::ClusterChainReader chainReader(fatTable, layout);

    const auto result = chainReader.getClusterChain(20);

    EXPECT_EQ(result.status, fat32tree::ClusterChainStatus::Complete);
    EXPECT_EQ(result.clusters, (std::vector<std::uint32_t>{20, 21, 40}));
}

TEST_F(ClusterChainReaderTest, RejectsFirstClusterBelowTwo)
{
    auto bytes = fatImage();
    writeImage(bytes);

    fat32tree::BinaryReader reader(m_imagePath);
    const auto layout = makeLayout();
    fat32tree::FatTableReader fatTable(reader, layout);
    const fat32tree::ClusterChainReader chainReader(fatTable, layout);

    const auto result = chainReader.getClusterChain(1);

    EXPECT_EQ(result.status, fat32tree::ClusterChainStatus::InvalidCluster);
    EXPECT_TRUE(result.clusters.empty());
}

TEST_F(ClusterChainReaderTest, RejectsFirstClusterAboveVolumeRange)
{
    auto bytes = fatImage();
    writeImage(bytes);

    fat32tree::BinaryReader reader(m_imagePath);
    const auto layout = makeLayout();
    fat32tree::FatTableReader fatTable(reader, layout);
    const fat32tree::ClusterChainReader chainReader(fatTable, layout);

    const auto result = chainReader.getClusterChain(102);

    EXPECT_EQ(result.status, fat32tree::ClusterChainStatus::InvalidCluster);
    EXPECT_TRUE(result.clusters.empty());
}

TEST_F(ClusterChainReaderTest, PreservesPrefixWhenNextClusterIsOutsideVolume)
{
    auto bytes = fatImage();
    writeU32LE(bytes, fatEntryOffset(20), 102);
    writeImage(bytes);

    fat32tree::BinaryReader reader(m_imagePath);
    const auto layout = makeLayout();
    fat32tree::FatTableReader fatTable(reader, layout);
    const fat32tree::ClusterChainReader chainReader(fatTable, layout);

    const auto result = chainReader.getClusterChain(20);

    EXPECT_EQ(result.status, fat32tree::ClusterChainStatus::InvalidCluster);
    EXPECT_EQ(result.clusters, (std::vector<std::uint32_t>{20}));
}

TEST_F(ClusterChainReaderTest, FreeEntryTerminatesAndPreservesPrefix)
{
    auto bytes = fatImage();
    writeU32LE(bytes, fatEntryOffset(20), 21);
    writeU32LE(bytes, fatEntryOffset(21), 0);
    writeImage(bytes);

    fat32tree::BinaryReader reader(m_imagePath);
    const auto layout = makeLayout();
    fat32tree::FatTableReader fatTable(reader, layout);
    const fat32tree::ClusterChainReader chainReader(fatTable, layout);

    const auto result = chainReader.getClusterChain(20);

    EXPECT_EQ(result.status, fat32tree::ClusterChainStatus::FreeCluster);
    EXPECT_EQ(result.clusters, (std::vector<std::uint32_t>{20, 21}));
}

TEST_F(ClusterChainReaderTest, BadEntryTerminatesAndPreservesPrefix)
{
    auto bytes = fatImage();
    writeU32LE(bytes, fatEntryOffset(20), 21);
    writeU32LE(bytes, fatEntryOffset(21), 0x0FFFFFF7);
    writeImage(bytes);

    fat32tree::BinaryReader reader(m_imagePath);
    const auto layout = makeLayout();
    fat32tree::FatTableReader fatTable(reader, layout);
    const fat32tree::ClusterChainReader chainReader(fatTable, layout);

    const auto result = chainReader.getClusterChain(20);

    EXPECT_EQ(result.status, fat32tree::ClusterChainStatus::BadCluster);
    EXPECT_EQ(result.clusters, (std::vector<std::uint32_t>{20, 21}));
}

TEST_F(ClusterChainReaderTest, ReservedEntryTerminatesAndPreservesPrefix)
{
    auto bytes = fatImage();
    writeU32LE(bytes, fatEntryOffset(20), 21);
    writeU32LE(bytes, fatEntryOffset(21), 0x0FFFFFF0);
    writeImage(bytes);

    fat32tree::BinaryReader reader(m_imagePath);
    const auto layout = makeLayout();
    fat32tree::FatTableReader fatTable(reader, layout);
    const fat32tree::ClusterChainReader chainReader(fatTable, layout);

    const auto result = chainReader.getClusterChain(20);

    EXPECT_EQ(result.status, fat32tree::ClusterChainStatus::ReservedValue);
    EXPECT_EQ(result.clusters, (std::vector<std::uint32_t>{20, 21}));
}

TEST_F(ClusterChainReaderTest, DetectsImmediateSelfCycleAndPreservesPrefix)
{
    auto bytes = fatImage();
    writeU32LE(bytes, fatEntryOffset(20), 20);
    writeImage(bytes);

    fat32tree::BinaryReader reader(m_imagePath);
    const auto layout = makeLayout();
    fat32tree::FatTableReader fatTable(reader, layout);
    const fat32tree::ClusterChainReader chainReader(fatTable, layout);

    const auto result = chainReader.getClusterChain(20);

    EXPECT_EQ(result.status, fat32tree::ClusterChainStatus::CycleDetected);
    EXPECT_EQ(result.clusters, (std::vector<std::uint32_t>{20}));
}

TEST_F(ClusterChainReaderTest, DetectsNonTrivialCycleAndPreservesPrefix)
{
    auto bytes = fatImage();
    writeU32LE(bytes, fatEntryOffset(20), 21);
    writeU32LE(bytes, fatEntryOffset(21), 40);
    writeU32LE(bytes, fatEntryOffset(40), 21);
    writeImage(bytes);

    fat32tree::BinaryReader reader(m_imagePath);
    const auto layout = makeLayout();
    fat32tree::FatTableReader fatTable(reader, layout);
    const fat32tree::ClusterChainReader chainReader(fatTable, layout);

    const auto result = chainReader.getClusterChain(20);

    EXPECT_EQ(result.status, fat32tree::ClusterChainStatus::CycleDetected);
    EXPECT_EQ(result.clusters, (std::vector<std::uint32_t>{20, 21, 40}));
}

TEST_F(ClusterChainReaderTest, FatReadErrorPreservesRecoveredPrefix)
{
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(kSelectedFatOffset + 20 * 4 + 4), 0);
    writeU32LE(bytes, fatEntryOffset(20), 21);
    writeImage(bytes);

    fat32tree::BinaryReader reader(m_imagePath);
    const auto layout = makeLayout();
    fat32tree::FatTableReader fatTable(reader, layout);
    const fat32tree::ClusterChainReader chainReader(fatTable, layout);

    const auto result = chainReader.getClusterChain(20);

    EXPECT_EQ(result.status, fat32tree::ClusterChainStatus::ReadError);
    EXPECT_EQ(result.clusters, (std::vector<std::uint32_t>{20, 21}));
}

} // namespace
