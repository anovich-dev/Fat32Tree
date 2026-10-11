#include "Mocks/ClusterChainReaderMock.h"
#include "Mocks/DirectoryEntryParserMock.h"
#include "fat32tree/boot/Fat32VolumeLayout.h"
#include "fat32tree/core/Fat32Constants.h"
#include "fat32tree/directory/DirectoryReadResult.h"
#include "fat32tree/directory/DirectoryReader.h"
#include "fat32tree/directory/IDirectoryEntryParser.h"
#include "fat32tree/fat/IClusterChainReader.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace
{

using ::testing::Return;
using ChainReaderMock = ::testing::StrictMock<fat32tree::test::ClusterChainReaderMock>;
using EntryParserMock = ::testing::StrictMock<fat32tree::test::DirectoryEntryParserMock>;

class DirectoryReaderTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_layout.dataOffset = 1000;
        m_layout.clusterSize = 64;

        auto chainMock = std::make_unique<ChainReaderMock>();
        m_chainReaderMock = chainMock.get();
        auto parserMock = std::make_unique<EntryParserMock>();
        m_entryParserMock = parserMock.get();

        std::unique_ptr<fat32tree::IClusterChainReader> chainReader = std::move(chainMock);
        std::unique_ptr<fat32tree::IDirectoryEntryParser> entryParser = std::move(parserMock);
        m_directoryReader = std::make_unique<fat32tree::DirectoryReader>(
            m_layout, std::move(chainReader), std::move(entryParser));
    }

    void expectChain(std::vector<std::uint32_t> clusters,
        fat32tree::ClusterChainStatus status = fat32tree::ClusterChainStatus::Complete,
        std::uint32_t firstCluster = 5)
    {
        EXPECT_CALL(*m_chainReaderMock, getClusterChain(firstCluster))
            .WillOnce(Return(fat32tree::ClusterChainResult{std::move(clusters), status}));
    }

    void expectParsed(std::uint64_t offset, fat32tree::DirectoryEntryState state,
        fat32tree::DirectoryEntry entry = {})
    {
        EXPECT_CALL(*m_entryParserMock, parse(offset))
            .WillOnce(Return(fat32tree::ParsedDirectoryEntry{state, std::move(entry)}));
    }

    static fat32tree::DirectoryEntry makeEntry(std::string name,
        fat32tree::DirectoryEntryType type = fat32tree::DirectoryEntryType::File,
        std::uint32_t firstCluster = 0, std::uint32_t fileSize = 0)
    {
        return {std::move(name), type, firstCluster, fileSize};
    }

    static constexpr std::uint64_t offsetForCluster(std::uint32_t cluster)
    {
        return 1000 + static_cast<std::uint64_t>(cluster - 2) * 64;
    }

protected:
    fat32tree::Fat32VolumeLayout m_layout;
    ChainReaderMock* m_chainReaderMock = nullptr;
    EntryParserMock* m_entryParserMock = nullptr;
    std::unique_ptr<fat32tree::DirectoryReader> m_directoryReader;
};

TEST_F(DirectoryReaderTest, ReadsNormalEntriesFromSingleCluster)
{
    expectChain({5});
    expectParsed(offsetForCluster(5), fat32tree::DirectoryEntryState::Normal,
        makeEntry("FIRST.TXT", fat32tree::DirectoryEntryType::File, 7, 12));
    expectParsed(offsetForCluster(5) + 32, fat32tree::DirectoryEntryState::Normal,
        makeEntry("SECOND", fat32tree::DirectoryEntryType::Directory, 8));

    const auto result = m_directoryReader->readDirectory(5);

    ASSERT_EQ(result.entries.size(), 2U);
    EXPECT_EQ(result.entries[0].name, "FIRST.TXT");
    EXPECT_EQ(result.entries[1].name, "SECOND");
    EXPECT_EQ(result.chainStatus, fat32tree::ClusterChainStatus::Complete);
}

TEST_F(DirectoryReaderTest, PreservesDirectoryAndFileEntries)
{
    expectChain({5});
    expectParsed(offsetForCluster(5), fat32tree::DirectoryEntryState::Normal,
        makeEntry("FILE.BIN", fat32tree::DirectoryEntryType::File, 21, 500));
    expectParsed(offsetForCluster(5) + 32, fat32tree::DirectoryEntryState::Normal,
        makeEntry("SUBDIR", fat32tree::DirectoryEntryType::Directory, 40, 0));

    const auto result = m_directoryReader->readDirectory(5);

    ASSERT_EQ(result.entries.size(), 2U);
    EXPECT_EQ(result.entries[0].type, fat32tree::DirectoryEntryType::File);
    EXPECT_EQ(result.entries[0].name, "FILE.BIN");
    EXPECT_EQ(result.entries[0].firstCluster, 21U);
    EXPECT_EQ(result.entries[0].fileSize, 500U);
    EXPECT_EQ(result.entries[1].type, fat32tree::DirectoryEntryType::Directory);
    EXPECT_EQ(result.entries[1].name, "SUBDIR");
    EXPECT_EQ(result.entries[1].firstCluster, 40U);
    EXPECT_EQ(result.entries[1].fileSize, 0U);
}

TEST_F(DirectoryReaderTest, ReturnsEmptyEntriesWhenDirectoryEndsImmediately)
{
    expectChain({5});
    expectParsed(offsetForCluster(5), fat32tree::DirectoryEntryState::End);

    const auto result = m_directoryReader->readDirectory(5);

    EXPECT_TRUE(result.entries.empty());
}

TEST_F(DirectoryReaderTest, SkipsDeletedEntries)
{
    expectChain({5});
    expectParsed(offsetForCluster(5), fat32tree::DirectoryEntryState::Deleted);
    expectParsed(
        offsetForCluster(5) + 32, fat32tree::DirectoryEntryState::Normal, makeEntry("KEEP.TXT"));

    const auto result = m_directoryReader->readDirectory(5);

    ASSERT_EQ(result.entries.size(), 1U);
    EXPECT_EQ(result.entries.front().name, "KEEP.TXT");
}

TEST_F(DirectoryReaderTest, SkipsLongFileNameEntries)
{
    expectChain({5});
    expectParsed(offsetForCluster(5), fat32tree::DirectoryEntryState::LongFileName);
    expectParsed(
        offsetForCluster(5) + 32, fat32tree::DirectoryEntryState::Normal, makeEntry("KEEP.TXT"));

    const auto result = m_directoryReader->readDirectory(5);

    ASSERT_EQ(result.entries.size(), 1U);
    EXPECT_EQ(result.entries.front().name, "KEEP.TXT");
}

TEST_F(DirectoryReaderTest, SkipsInvalidEntriesAndContinuesReading)
{
    expectChain({5, 6});
    expectParsed(offsetForCluster(5), fat32tree::DirectoryEntryState::Normal, makeEntry("FILE1"));
    expectParsed(offsetForCluster(5) + 32, fat32tree::DirectoryEntryState::Invalid);
    expectParsed(offsetForCluster(6), fat32tree::DirectoryEntryState::Normal, makeEntry("FILE2"));
    expectParsed(offsetForCluster(6) + 32, fat32tree::DirectoryEntryState::End);

    const auto result = m_directoryReader->readDirectory(5);

    ASSERT_EQ(result.entries.size(), 2U);
    EXPECT_EQ(result.entries[0].name, "FILE1");
    EXPECT_EQ(result.entries[1].name, "FILE2");
}

TEST_F(DirectoryReaderTest, SkipsDotEntry)
{
    expectChain({5});
    expectParsed(offsetForCluster(5), fat32tree::DirectoryEntryState::Normal,
        makeEntry(".", fat32tree::DirectoryEntryType::Directory));
    expectParsed(offsetForCluster(5) + 32, fat32tree::DirectoryEntryState::Normal,
        makeEntry("CHILD", fat32tree::DirectoryEntryType::Directory));

    const auto result = m_directoryReader->readDirectory(5);

    ASSERT_EQ(result.entries.size(), 1U);
    EXPECT_EQ(result.entries.front().name, "CHILD");
}

TEST_F(DirectoryReaderTest, SkipsDotDotEntry)
{
    expectChain({5});
    expectParsed(offsetForCluster(5), fat32tree::DirectoryEntryState::Normal,
        makeEntry("..", fat32tree::DirectoryEntryType::Directory));
    expectParsed(offsetForCluster(5) + 32, fat32tree::DirectoryEntryState::Normal,
        makeEntry("CHILD", fat32tree::DirectoryEntryType::Directory));

    const auto result = m_directoryReader->readDirectory(5);

    ASSERT_EQ(result.entries.size(), 1U);
    EXPECT_EQ(result.entries.front().name, "CHILD");
}

TEST_F(DirectoryReaderTest, SkipsVolumeLabelForRegularDirectory)
{
    expectChain({5});
    expectParsed(offsetForCluster(5), fat32tree::DirectoryEntryState::VolumeLabel,
        makeEntry("MY_USB", fat32tree::DirectoryEntryType::Directory));
    expectParsed(
        offsetForCluster(5) + 32, fat32tree::DirectoryEntryState::Normal, makeEntry("CHILD"));

    const auto result = m_directoryReader->readDirectory(5);

    ASSERT_EQ(result.entries.size(), 1U);
    EXPECT_EQ(result.entries.front().name, "CHILD");
}

TEST_F(DirectoryReaderTest, PreservesVolumeLabelWhenReadingRootDirectory)
{
    expectChain({5});
    expectParsed(offsetForCluster(5), fat32tree::DirectoryEntryState::VolumeLabel,
        makeEntry("MY_USB", fat32tree::DirectoryEntryType::Directory));
    expectParsed(
        offsetForCluster(5) + 32, fat32tree::DirectoryEntryState::Normal, makeEntry("README.TXT"));

    const auto result = m_directoryReader->readRootDirectory(5);

    ASSERT_TRUE(result.volumeLabel.has_value());
    EXPECT_EQ(*result.volumeLabel, "MY_USB");
    ASSERT_EQ(result.entries.size(), 1U);
    EXPECT_EQ(result.entries.front().name, "README.TXT");
}

TEST_F(DirectoryReaderTest, ReturnsEmptyVolumeLabelWhenRootHasNoVolumeLabel)
{
    expectChain({5});
    expectParsed(
        offsetForCluster(5), fat32tree::DirectoryEntryState::Normal, makeEntry("README.TXT"));
    expectParsed(offsetForCluster(5) + 32, fat32tree::DirectoryEntryState::End);

    const auto result = m_directoryReader->readRootDirectory(5);

    EXPECT_FALSE(result.volumeLabel.has_value());
    ASSERT_EQ(result.entries.size(), 1U);
}

TEST_F(DirectoryReaderTest, PreservesFirstVolumeLabelWhenMultipleLabelsAreFound)
{
    expectChain({5, 6});
    expectParsed(offsetForCluster(5), fat32tree::DirectoryEntryState::VolumeLabel,
        makeEntry("FIRST", fat32tree::DirectoryEntryType::Directory));
    expectParsed(offsetForCluster(5) + 32, fat32tree::DirectoryEntryState::VolumeLabel,
        makeEntry("SECOND", fat32tree::DirectoryEntryType::Directory));
    expectParsed(offsetForCluster(6), fat32tree::DirectoryEntryState::End);

    const auto result = m_directoryReader->readRootDirectory(5);

    ASSERT_TRUE(result.volumeLabel.has_value());
    EXPECT_EQ(*result.volumeLabel, "FIRST");
}

TEST_F(DirectoryReaderTest, StopsReadingAfterEndEntry)
{
    expectChain({5});
    expectParsed(
        offsetForCluster(5), fat32tree::DirectoryEntryState::Normal, makeEntry("BEFORE_END"));
    expectParsed(offsetForCluster(5) + 32, fat32tree::DirectoryEntryState::End);

    const auto result = m_directoryReader->readDirectory(5);

    ASSERT_EQ(result.entries.size(), 1U);
    EXPECT_EQ(result.entries.front().name, "BEFORE_END");
}

TEST_F(DirectoryReaderTest, DoesNotReadFollowingClustersAfterEndEntry)
{
    expectChain({5, 9});
    expectParsed(
        offsetForCluster(5), fat32tree::DirectoryEntryState::Normal, makeEntry("BEFORE_END"));
    expectParsed(offsetForCluster(5) + 32, fat32tree::DirectoryEntryState::End);

    const auto result = m_directoryReader->readDirectory(5);

    ASSERT_EQ(result.entries.size(), 1U);
    EXPECT_EQ(result.entries.front().name, "BEFORE_END");
}

TEST_F(DirectoryReaderTest, ReadsEntriesAcrossMultipleClusters)
{
    expectChain({5, 6});
    expectParsed(offsetForCluster(5), fat32tree::DirectoryEntryState::Normal, makeEntry("FILE1"));
    expectParsed(offsetForCluster(5) + 32, fat32tree::DirectoryEntryState::Deleted);
    expectParsed(offsetForCluster(6), fat32tree::DirectoryEntryState::Normal, makeEntry("FILE2"));
    expectParsed(offsetForCluster(6) + 32, fat32tree::DirectoryEntryState::End);

    const auto result = m_directoryReader->readDirectory(5);

    ASSERT_EQ(result.entries.size(), 2U);
    EXPECT_EQ(result.entries[0].name, "FILE1");
    EXPECT_EQ(result.entries[1].name, "FILE2");
}

TEST_F(DirectoryReaderTest, ReadsFragmentedDirectoryChainInFatOrder)
{
    expectChain({20, 900, 31});
    expectParsed(offsetForCluster(20), fat32tree::DirectoryEntryState::Normal, makeEntry("FILE1"));
    expectParsed(offsetForCluster(20) + 32, fat32tree::DirectoryEntryState::Deleted);
    expectParsed(offsetForCluster(900), fat32tree::DirectoryEntryState::Normal, makeEntry("FILE2"));
    expectParsed(offsetForCluster(900) + 32, fat32tree::DirectoryEntryState::Deleted);
    expectParsed(offsetForCluster(31), fat32tree::DirectoryEntryState::Normal, makeEntry("FILE3"));
    expectParsed(offsetForCluster(31) + 32, fat32tree::DirectoryEntryState::End);

    const auto result = m_directoryReader->readDirectory(5);

    ASSERT_EQ(result.entries.size(), 3U);
    EXPECT_EQ(result.entries[0].name, "FILE1");
    EXPECT_EQ(result.entries[1].name, "FILE2");
    EXPECT_EQ(result.entries[2].name, "FILE3");
}

TEST_F(DirectoryReaderTest, CalculatesEntryOffsetsFromClusterGeometry)
{
    expectChain({5});
    expectParsed(1192, fat32tree::DirectoryEntryState::Normal, makeEntry("OFFSET1"));
    expectParsed(1224, fat32tree::DirectoryEntryState::Normal, makeEntry("OFFSET2"));

    const auto result = m_directoryReader->readDirectory(5);

    ASSERT_EQ(result.entries.size(), 2U);
}

TEST_F(DirectoryReaderTest, PreservesEntriesFromPartialClusterChain)
{
    expectChain({5, 9}, fat32tree::ClusterChainStatus::BadCluster);
    expectParsed(
        offsetForCluster(5), fat32tree::DirectoryEntryState::Normal, makeEntry("RECOVERED1"));
    expectParsed(offsetForCluster(5) + 32, fat32tree::DirectoryEntryState::Deleted);
    expectParsed(
        offsetForCluster(9), fat32tree::DirectoryEntryState::Normal, makeEntry("RECOVERED2"));
    expectParsed(offsetForCluster(9) + 32, fat32tree::DirectoryEntryState::End);

    const auto result = m_directoryReader->readDirectory(5);

    ASSERT_EQ(result.entries.size(), 2U);
    EXPECT_EQ(result.entries[0].name, "RECOVERED1");
    EXPECT_EQ(result.entries[1].name, "RECOVERED2");
    EXPECT_EQ(result.chainStatus, fat32tree::ClusterChainStatus::BadCluster);
}

TEST_F(DirectoryReaderTest, PreservesEntriesWhenClusterChainCycleWasDetected)
{
    expectChain({5, 9, 14}, fat32tree::ClusterChainStatus::CycleDetected);
    expectParsed(
        offsetForCluster(5), fat32tree::DirectoryEntryState::Normal, makeEntry("RECOVERED1"));
    expectParsed(offsetForCluster(5) + 32, fat32tree::DirectoryEntryState::Deleted);
    expectParsed(
        offsetForCluster(9), fat32tree::DirectoryEntryState::Normal, makeEntry("RECOVERED2"));
    expectParsed(offsetForCluster(9) + 32, fat32tree::DirectoryEntryState::Deleted);
    expectParsed(
        offsetForCluster(14), fat32tree::DirectoryEntryState::Normal, makeEntry("RECOVERED3"));
    expectParsed(offsetForCluster(14) + 32, fat32tree::DirectoryEntryState::End);

    const auto result = m_directoryReader->readDirectory(5);

    ASSERT_EQ(result.entries.size(), 3U);
    EXPECT_EQ(result.chainStatus, fat32tree::ClusterChainStatus::CycleDetected);
}

TEST_F(DirectoryReaderTest, ReturnsEmptyEntriesForInvalidFirstCluster)
{
    expectChain({}, fat32tree::ClusterChainStatus::InvalidCluster, 0);

    const auto result = m_directoryReader->readDirectory(0);

    EXPECT_TRUE(result.entries.empty());
    EXPECT_EQ(result.chainStatus, fat32tree::ClusterChainStatus::InvalidCluster);
}

} // namespace
