#include "fat32tree/BinaryReader.h"
#include "fat32tree/ClusterChainReader.h"
#include "fat32tree/DirectoryEntryParser.h"
#include "fat32tree/DirectoryEntryValidator.h"
#include "fat32tree/DirectoryReader.h"
#include "fat32tree/Fat32Constants.h"
#include "fat32tree/Fat32VolumeLayout.h"
#include "fat32tree/FatTableReader.h"
#include "fat32tree/FileNode.h"
#include "fat32tree/FileTreeBuilder.h"

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
#include <utility>
#include <vector>

#include <gtest/gtest.h>

namespace
{

namespace fat = fat32tree::fat32;
using fat32tree::DirectoryEntryType;

class FileTreeBuilderIntegrationTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Deliberately bypass BPB parsing: a small image with one FAT and
        // 64 data clusters is enough to exercise the real tree-reading stack.
        m_layout.bytesPerSector = 512;
        m_layout.sectorsPerCluster = 1;
        m_layout.clusterSize = 512;
        m_layout.fatCount = 1;
        m_layout.fatSizeSectors = 1;
        m_layout.fatSizeBytes = 512;
        m_layout.fatOffset = 512;
        m_layout.selectedFatOffset = 512;
        m_layout.firstDataSector = 2;
        m_layout.dataOffset = 1024;
        m_layout.clusterCount = 64;
        m_layout.dataSectors = 64;
        m_layout.totalSectors = 66;
        m_layout.volumeSize = 66 * 512;
        m_layout.rootCluster = 2;
        m_image.resize(static_cast<std::size_t>(m_layout.volumeSize), 0);

        static std::atomic<std::uint64_t> nextId{0};
        const auto timestamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        const auto temporaryRoot = std::filesystem::temp_directory_path();
        for (int attempt = 0; attempt < 100; ++attempt)
        {
            const auto candidate =
                temporaryRoot / ("Fat32Tree-FileTreeIntegration-" + std::to_string(timestamp) +
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
        m_imagePath = m_temporaryDirectory / "tree.img";
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

    void writeLE(std::size_t offset, std::uint32_t value, std::size_t byteCount)
    {
        for (std::size_t i = 0; i < byteCount; ++i)
        {
            m_image.at(offset + i) = static_cast<std::uint8_t>(value >> (8 * i));
        }
    }

    void writeFatEntry(std::uint32_t cluster, std::uint32_t value)
    {
        writeLE(static_cast<std::size_t>(m_layout.selectedFatOffset) + cluster * fat::kFatEntrySize,
            value, 4);
    }

    std::size_t clusterOffset(std::uint32_t cluster) const
    {
        return static_cast<std::size_t>(m_layout.dataOffset + (cluster - 2) * m_layout.clusterSize);
    }

    void writeEntry(std::uint32_t cluster, std::size_t index, std::string_view rawName,
        std::uint8_t attributes, std::uint32_t firstCluster = 0, std::uint32_t fileSize = 0)
    {
        ASSERT_EQ(rawName.size(), fat::kShortNameLength);
        ASSERT_LT(index, m_layout.clusterSize / fat::kDirectoryEntrySize);
        const auto offset = clusterOffset(cluster) + index * fat::kDirectoryEntrySize;
        for (std::size_t i = 0; i < rawName.size(); ++i)
        {
            m_image.at(offset + i) = static_cast<std::uint8_t>(rawName[i]);
        }
        m_image.at(offset + fat::kDirectoryAttributeOffset) = attributes;
        writeLE(offset + fat::kDirectoryFirstClusterHighOffset, firstCluster >> 16, 2);
        writeLE(offset + fat::kDirectoryFirstClusterLowOffset, firstCluster, 2);
        writeLE(offset + fat::kDirectoryFileSizeOffset, fileSize, 4);
    }

    void fillWithDeletedEntries(std::uint32_t cluster)
    {
        // No 0x00 end marker before the next cluster in the directory chain.
        for (std::size_t i = 0; i < m_layout.clusterSize / fat::kDirectoryEntrySize; ++i)
        {
            m_image.at(clusterOffset(cluster) + i * fat::kDirectoryEntrySize) =
                fat::kDirectoryEntryDeletedMarker;
        }
    }

    void writeDotEntries(std::uint32_t cluster, std::uint32_t parent)
    {
        writeEntry(cluster, 0, ".          ", fat::kAttributeDirectory, cluster);
        writeEntry(cluster, 1, "..         ", fat::kAttributeDirectory, parent);
    }

    void saveImage()
    {
        std::ofstream image(m_imagePath, std::ios::binary);
        ASSERT_TRUE(image.is_open());
        image.write(reinterpret_cast<const char*>(m_image.data()),
            static_cast<std::streamsize>(m_image.size()));
        image.close();
        ASSERT_TRUE(image.good());
    }

    std::unique_ptr<fat32tree::DirectoryReader> makeDirectoryReader(
        fat32tree::BinaryReader& reader, fat32tree::FatTableReader& fatTableReader)
    {
        return std::make_unique<fat32tree::DirectoryReader>(m_layout,
            std::make_unique<fat32tree::ClusterChainReader>(fatTableReader, m_layout),
            std::make_unique<fat32tree::DirectoryEntryParser>(
                reader, std::make_unique<fat32tree::DirectoryEntryValidator>()));
    }

    static void expectNode(const fat32tree::FileNode& node, std::string_view name,
        DirectoryEntryType type, std::uint32_t firstCluster, std::uint32_t fileSize,
        std::size_t childCount)
    {
        SCOPED_TRACE(name);
        EXPECT_EQ(node.name, name);
        EXPECT_EQ(node.type, type);
        EXPECT_EQ(node.firstCluster, firstCluster);
        EXPECT_EQ(node.fileSize, fileSize);
        EXPECT_EQ(node.children.size(), childCount);
    }

protected:
    fat32tree::Fat32VolumeLayout m_layout{};
    std::vector<std::uint8_t> m_image;
    std::filesystem::path m_temporaryDirectory;
    std::filesystem::path m_imagePath;
};

TEST_F(FileTreeBuilderIntegrationTest, BuildsNestedTreeFromFragmentedDirectoryChains)
{
    writeFatEntry(2, fat::kEndOfChainMax);
    writeFatEntry(20, 35);
    writeFatEntry(35, fat::kEndOfChainMax);
    writeFatEntry(30, fat::kEndOfChainMax);
    writeFatEntry(50, fat::kEndOfChainMax);
    writeFatEntry(7, fat::kEndOfChainMax);
    writeFatEntry(8, fat::kEndOfChainMax);
    writeFatEntry(9, fat::kEndOfChainMax);

    writeEntry(2, 0, "MY_USB     ", fat::kAttributeVolumeId);
    writeEntry(2, 1, "README  TXT", fat::kAttributeArchive, 7, 18);
    writeEntry(2, 2, "PHOTOS     ", fat::kAttributeDirectory, 20);
    writeEntry(2, 3, "EMPTYDIR   ", fat::kAttributeDirectory, 50);

    fillWithDeletedEntries(20);
    writeDotEntries(20, 2);
    writeEntry(20, 2, "A       JPG", fat::kAttributeArchive, 8, 1234);
    // TRIP exists only in the non-contiguous second cluster, so discovering
    // B.JPG requires both following the FAT link and descending another level.
    writeEntry(35, 0, "TRIP       ", fat::kAttributeDirectory, 30);
    writeDotEntries(30, 20);
    writeEntry(30, 2, "B       JPG", fat::kAttributeArchive, 9, 5678);
    writeDotEntries(50, 2);
    ASSERT_NO_FATAL_FAILURE(saveImage());

    fat32tree::BinaryReader reader(m_imagePath);
    ASSERT_EQ(reader.size(), m_layout.volumeSize);
    fat32tree::FatTableReader fatTableReader(reader, m_layout);
    const fat32tree::ClusterChainReader chainReader(fatTableReader, m_layout);
    const auto chain = chainReader.getClusterChain(20);
    EXPECT_EQ(chain.status, fat32tree::ClusterChainStatus::Complete);
    EXPECT_EQ(chain.clusters, (std::vector<std::uint32_t>{20, 35}));

    fat32tree::FileTreeBuilder builder(m_layout, makeDirectoryReader(reader, fatTableReader));
    const auto root = builder.buildTree();

    expectNode(root, "MY_USB", DirectoryEntryType::Directory, 2, 0, 3);
    ASSERT_EQ(root.children.size(), 3U);
    expectNode(*root.children[0], "README.TXT", DirectoryEntryType::File, 7, 18, 0);
    const auto& photos = *root.children[1];
    expectNode(photos, "PHOTOS", DirectoryEntryType::Directory, 20, 0, 2);
    ASSERT_EQ(photos.children.size(), 2U);
    expectNode(*photos.children[0], "A.JPG", DirectoryEntryType::File, 8, 1234, 0);
    const auto& trip = *photos.children[1];
    expectNode(trip, "TRIP", DirectoryEntryType::Directory, 30, 0, 1);
    ASSERT_EQ(trip.children.size(), 1U);
    expectNode(*trip.children[0], "B.JPG", DirectoryEntryType::File, 9, 5678, 0);
    expectNode(*root.children[2], "EMPTYDIR", DirectoryEntryType::Directory, 50, 0, 0);
}

TEST_F(FileTreeBuilderIntegrationTest, BuildsPartialTreeWhenDirectoryChainIsCorrupted)
{
    writeFatEntry(2, fat::kEndOfChainMax);
    writeFatEntry(40, 41);
    writeFatEntry(41, fat::kBadClusterValue);
    writeFatEntry(7, fat::kEndOfChainMax);
    writeFatEntry(8, fat::kEndOfChainMax);
    writeEntry(2, 0, "MY_USB     ", fat::kAttributeVolumeId);
    writeEntry(2, 1, "BROKEN     ", fat::kAttributeDirectory, 40);
    fillWithDeletedEntries(40);
    writeDotEntries(40, 2);
    writeEntry(40, 2, "FILE1   TXT", fat::kAttributeArchive, 7, 111);
    writeEntry(41, 0, "FILE2   TXT", fat::kAttributeArchive, 8, 222);
    ASSERT_NO_FATAL_FAILURE(saveImage());

    fat32tree::BinaryReader reader(m_imagePath);
    fat32tree::FatTableReader fatTableReader(reader, m_layout);
    const fat32tree::ClusterChainReader chainReader(fatTableReader, m_layout);
    const auto chain = chainReader.getClusterChain(40);
    EXPECT_EQ(chain.status, fat32tree::ClusterChainStatus::BadCluster);
    EXPECT_EQ(chain.clusters, (std::vector<std::uint32_t>{40, 41}));

    auto directoryReader = makeDirectoryReader(reader, fatTableReader);
    const auto recoveredDirectory = directoryReader->readDirectory(40);
    EXPECT_EQ(recoveredDirectory.chainStatus, fat32tree::ClusterChainStatus::BadCluster);
    ASSERT_EQ(recoveredDirectory.entries.size(), 2U);
    EXPECT_EQ(recoveredDirectory.entries[0].name, "FILE1.TXT");
    EXPECT_EQ(recoveredDirectory.entries[1].name, "FILE2.TXT");

    fat32tree::FileTreeBuilder builder(m_layout, std::move(directoryReader));
    const auto root = builder.buildTree();

    expectNode(root, "MY_USB", DirectoryEntryType::Directory, 2, 0, 1);
    ASSERT_EQ(root.children.size(), 1U);
    const auto& broken = *root.children[0];
    expectNode(broken, "BROKEN", DirectoryEntryType::Directory, 40, 0, 2);
    ASSERT_EQ(broken.children.size(), 2U);
    expectNode(*broken.children[0], "FILE1.TXT", DirectoryEntryType::File, 7, 111, 0);
    expectNode(*broken.children[1], "FILE2.TXT", DirectoryEntryType::File, 8, 222, 0);
}

TEST_F(FileTreeBuilderIntegrationTest, BuildsPartialTreeWhenDirectoryChainContainsCycle)
{
    writeFatEntry(2, fat::kEndOfChainMax);
    writeFatEntry(20, 21);
    writeFatEntry(21, 40);
    writeFatEntry(40, 21);
    writeFatEntry(7, fat::kEndOfChainMax);
    writeFatEntry(8, fat::kEndOfChainMax);
    writeFatEntry(9, fat::kEndOfChainMax);

    writeEntry(2, 0, "MY_USB     ", fat::kAttributeVolumeId);
    writeEntry(2, 1, "CYCLIC     ", fat::kAttributeDirectory, 20);

    // Leave no End marker in the directory chain: scanning must terminate
    // because the FAT cycle was detected, rather than because of a free slot.
    fillWithDeletedEntries(20);
    fillWithDeletedEntries(21);
    fillWithDeletedEntries(40);
    writeDotEntries(20, 2);
    writeEntry(20, 2, "FILE1   TXT", fat::kAttributeArchive, 7, 111);
    writeEntry(21, 0, "FILE2   TXT", fat::kAttributeArchive, 8, 222);
    writeEntry(40, 0, "FILE3   TXT", fat::kAttributeArchive, 9, 333);
    ASSERT_NO_FATAL_FAILURE(saveImage());

    fat32tree::BinaryReader reader(m_imagePath);
    fat32tree::FatTableReader fatTableReader(reader, m_layout);
    const fat32tree::ClusterChainReader chainReader(fatTableReader, m_layout);
    const auto chain = chainReader.getClusterChain(20);

    EXPECT_EQ(chain.status, fat32tree::ClusterChainStatus::CycleDetected);
    EXPECT_EQ(chain.clusters, (std::vector<std::uint32_t>{20, 21, 40}));

    fat32tree::FileTreeBuilder builder(m_layout, makeDirectoryReader(reader, fatTableReader));
    const auto root = builder.buildTree();

    expectNode(root, "MY_USB", DirectoryEntryType::Directory, 2, 0, 1);
    ASSERT_EQ(root.children.size(), 1U);
    const auto& cyclic = *root.children[0];
    expectNode(cyclic, "CYCLIC", DirectoryEntryType::Directory, 20, 0, 3);
    ASSERT_EQ(cyclic.children.size(), 3U);
    expectNode(*cyclic.children[0], "FILE1.TXT", DirectoryEntryType::File, 7, 111, 0);
    expectNode(*cyclic.children[1], "FILE2.TXT", DirectoryEntryType::File, 8, 222, 0);
    expectNode(*cyclic.children[2], "FILE3.TXT", DirectoryEntryType::File, 9, 333, 0);
}

TEST_F(FileTreeBuilderIntegrationTest, KeepsDirectoryNodeWithInvalidZeroClusterWithoutTraversingIt)
{
    writeFatEntry(m_layout.rootCluster, fat::kEndOfChainMax);
    writeEntry(m_layout.rootCluster, 0, "MY_USB     ", fat::kAttributeVolumeId);
    writeEntry(m_layout.rootCluster, 1, "BROKEN     ", fat::kAttributeDirectory, 0);
    ASSERT_NO_FATAL_FAILURE(saveImage());

    fat32tree::BinaryReader reader(m_imagePath);
    fat32tree::DirectoryEntryParser parser(
        reader, std::make_unique<fat32tree::DirectoryEntryValidator>());
    const auto parsed =
        parser.parse(clusterOffset(m_layout.rootCluster) + fat::kDirectoryEntrySize);

    // The parser accepts the readable entry; cluster validity is checked
    // later by the real chain reader rather than by short-name validation.
    ASSERT_EQ(parsed.state, fat32tree::DirectoryEntryState::Normal);
    EXPECT_EQ(parsed.entry.name, "BROKEN");
    EXPECT_EQ(parsed.entry.type, DirectoryEntryType::Directory);
    EXPECT_EQ(parsed.entry.firstCluster, 0U);

    fat32tree::FatTableReader fatTableReader(reader, m_layout);
    const fat32tree::ClusterChainReader chainReader(fatTableReader, m_layout);
    const auto chain = chainReader.getClusterChain(0);
    EXPECT_EQ(chain.status, fat32tree::ClusterChainStatus::InvalidCluster);
    EXPECT_TRUE(chain.clusters.empty());

    fat32tree::FileTreeBuilder builder(m_layout, makeDirectoryReader(reader, fatTableReader));
    const auto root = builder.buildTree();

    expectNode(root, "MY_USB", DirectoryEntryType::Directory, m_layout.rootCluster, 0, 1);
    ASSERT_EQ(root.children.size(), 1U);
    const auto& broken = *root.children[0];
    expectNode(broken, "BROKEN", DirectoryEntryType::Directory, 0, 0, 0);
    EXPECT_TRUE(broken.children.empty());
}

} // namespace
