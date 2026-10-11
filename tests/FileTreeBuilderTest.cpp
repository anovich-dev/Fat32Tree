#include <fat32tree/DirectoryReadResult.h>
#include <fat32tree/Fat32VolumeLayout.h>
#include <fat32tree/FileNode.h>
#include <fat32tree/FileTreeBuilder.h>
#include <fat32tree/IDirectoryReader.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "Mocks/DirectoryReaderMock.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace
{

using ::testing::_;
using ::testing::Return;
using StrictDirectoryReaderMock =
    ::testing::StrictMock<fat32tree::test::DirectoryReaderMock>;

class FileTreeBuilderTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_layout.rootCluster = 2;
        auto reader = std::make_unique<StrictDirectoryReaderMock>();
        m_directoryReaderMock = reader.get();
        std::unique_ptr<fat32tree::IDirectoryReader> directoryReader = std::move(reader);
        m_builder = std::make_unique<fat32tree::FileTreeBuilder>(
            m_layout,
            std::move(directoryReader));
    }

    static fat32tree::DirectoryEntry makeEntry(
        std::string name,
        fat32tree::DirectoryEntryType type,
        std::uint32_t firstCluster = 0,
        std::uint32_t fileSize = 0)
    {
        return {std::move(name), type, firstCluster, fileSize};
    }

    static fat32tree::DirectoryReadResult directoryResult(
        std::vector<fat32tree::DirectoryEntry> entries,
        fat32tree::ClusterChainStatus status = fat32tree::ClusterChainStatus::Complete)
    {
        return {std::move(entries), status};
    }

    static fat32tree::RootDirectoryReadResult rootResult(
        std::vector<fat32tree::DirectoryEntry> entries = {},
        std::optional<std::string> volumeLabel = std::nullopt,
        fat32tree::ClusterChainStatus status = fat32tree::ClusterChainStatus::Complete)
    {
        return {std::move(entries), std::move(volumeLabel), status};
    }

    fat32tree::Fat32VolumeLayout m_layout;
    StrictDirectoryReaderMock* m_directoryReaderMock = nullptr;
    std::unique_ptr<fat32tree::FileTreeBuilder> m_builder;
};

TEST_F(FileTreeBuilderTest, CreatesRootNodeFromVolumeLabel)
{
    EXPECT_CALL(*m_directoryReaderMock, readRootDirectory(2))
        .WillOnce(Return(rootResult({}, "MY_USB")));

    const auto root = m_builder->buildTree();

    EXPECT_EQ(root.name, "MY_USB");
    EXPECT_EQ(root.type, fat32tree::DirectoryEntryType::Directory);
    EXPECT_EQ(root.firstCluster, 2U);
    EXPECT_EQ(root.fileSize, 0U);
    EXPECT_TRUE(root.children.empty());
}

TEST_F(FileTreeBuilderTest, UsesSlashWhenVolumeLabelIsMissing)
{
    EXPECT_CALL(*m_directoryReaderMock, readRootDirectory(2))
        .WillOnce(Return(rootResult()));

    const auto root = m_builder->buildTree();

    EXPECT_EQ(root.name, "/");
    EXPECT_EQ(root.type, fat32tree::DirectoryEntryType::Directory);
    EXPECT_EQ(root.firstCluster, 2U);
}

TEST_F(FileTreeBuilderTest, CreatesEmptyTreeForEmptyRootDirectory)
{
    EXPECT_CALL(*m_directoryReaderMock, readRootDirectory(2))
        .WillOnce(Return(rootResult()));

    const auto root = m_builder->buildTree();

    ASSERT_TRUE(root.children.empty());
}

TEST_F(FileTreeBuilderTest, AddsFilesFromRootDirectory)
{
    EXPECT_CALL(*m_directoryReaderMock, readRootDirectory(2))
        .WillOnce(Return(rootResult({
            makeEntry("README.TXT", fat32tree::DirectoryEntryType::File, 7, 18),
            makeEntry("DATA.BIN", fat32tree::DirectoryEntryType::File, 8, 1024)})));

    const auto root = m_builder->buildTree();

    ASSERT_EQ(root.children.size(), 2U);
    EXPECT_EQ(root.children[0]->name, "README.TXT");
    EXPECT_EQ(root.children[0]->type, fat32tree::DirectoryEntryType::File);
    EXPECT_EQ(root.children[0]->firstCluster, 7U);
    EXPECT_EQ(root.children[0]->fileSize, 18U);
    EXPECT_EQ(root.children[1]->name, "DATA.BIN");
    EXPECT_EQ(root.children[1]->type, fat32tree::DirectoryEntryType::File);
    EXPECT_EQ(root.children[1]->firstCluster, 8U);
    EXPECT_EQ(root.children[1]->fileSize, 1024U);
}

TEST_F(FileTreeBuilderTest, AddsDirectoriesFromRootDirectory)
{
    EXPECT_CALL(*m_directoryReaderMock, readRootDirectory(2))
        .WillOnce(Return(rootResult({
            makeEntry("PHOTOS", fat32tree::DirectoryEntryType::Directory, 20)})));
    EXPECT_CALL(*m_directoryReaderMock, readDirectory(20))
        .WillOnce(Return(directoryResult({})));

    const auto root = m_builder->buildTree();

    ASSERT_EQ(root.children.size(), 1U);
    EXPECT_EQ(root.children[0]->name, "PHOTOS");
    EXPECT_EQ(root.children[0]->type, fat32tree::DirectoryEntryType::Directory);
    EXPECT_EQ(root.children[0]->firstCluster, 20U);
}

TEST_F(FileTreeBuilderTest, PreservesMixedFilesAndDirectoriesOrder)
{
    EXPECT_CALL(*m_directoryReaderMock, readRootDirectory(2))
        .WillOnce(Return(rootResult({
            makeEntry("README.TXT", fat32tree::DirectoryEntryType::File),
            makeEntry("PHOTOS", fat32tree::DirectoryEntryType::Directory, 20),
            makeEntry("DATA.BIN", fat32tree::DirectoryEntryType::File),
            makeEntry("DOCS", fat32tree::DirectoryEntryType::Directory, 30)})));
    EXPECT_CALL(*m_directoryReaderMock, readDirectory(20))
        .WillOnce(Return(directoryResult({})));
    EXPECT_CALL(*m_directoryReaderMock, readDirectory(30))
        .WillOnce(Return(directoryResult({})));

    const auto root = m_builder->buildTree();

    ASSERT_EQ(root.children.size(), 4U);
    EXPECT_EQ(root.children[0]->name, "README.TXT");
    EXPECT_EQ(root.children[1]->name, "PHOTOS");
    EXPECT_EQ(root.children[2]->name, "DATA.BIN");
    EXPECT_EQ(root.children[3]->name, "DOCS");
}

TEST_F(FileTreeBuilderTest, ReadsChildDirectoryAndAddsItsChildren)
{
    EXPECT_CALL(*m_directoryReaderMock, readRootDirectory(2))
        .WillOnce(Return(rootResult({
            makeEntry("PHOTOS", fat32tree::DirectoryEntryType::Directory, 20)})));
    EXPECT_CALL(*m_directoryReaderMock, readDirectory(20))
        .WillOnce(Return(directoryResult({
            makeEntry("A.JPG", fat32tree::DirectoryEntryType::File, 41, 100),
            makeEntry("B.JPG", fat32tree::DirectoryEntryType::File, 42, 200)})));

    const auto root = m_builder->buildTree();

    ASSERT_EQ(root.children.size(), 1U);
    ASSERT_EQ(root.children[0]->children.size(), 2U);
    EXPECT_EQ(root.children[0]->children[0]->name, "A.JPG");
    EXPECT_EQ(root.children[0]->children[1]->name, "B.JPG");
}

TEST_F(FileTreeBuilderTest, BuildsMultipleDirectoryLevels)
{
    EXPECT_CALL(*m_directoryReaderMock, readRootDirectory(2))
        .WillOnce(Return(rootResult({
            makeEntry("PHOTOS", fat32tree::DirectoryEntryType::Directory, 20)})));
    EXPECT_CALL(*m_directoryReaderMock, readDirectory(20))
        .WillOnce(Return(directoryResult({
            makeEntry("TRIP", fat32tree::DirectoryEntryType::Directory, 40)})));
    EXPECT_CALL(*m_directoryReaderMock, readDirectory(40))
        .WillOnce(Return(directoryResult({
            makeEntry("A.JPG", fat32tree::DirectoryEntryType::File, 80, 9)})));

    const auto root = m_builder->buildTree();

    ASSERT_EQ(root.children.size(), 1U);
    ASSERT_EQ(root.children[0]->children.size(), 1U);
    EXPECT_EQ(root.children[0]->children[0]->name, "TRIP");
    ASSERT_EQ(root.children[0]->children[0]->children.size(), 1U);
    EXPECT_EQ(root.children[0]->children[0]->children[0]->name, "A.JPG");
}

TEST_F(FileTreeBuilderTest, BuildsIndependentSiblingDirectories)
{
    EXPECT_CALL(*m_directoryReaderMock, readRootDirectory(2))
        .WillOnce(Return(rootResult({
            makeEntry("PHOTOS", fat32tree::DirectoryEntryType::Directory, 20),
            makeEntry("DOCS", fat32tree::DirectoryEntryType::Directory, 50)})));
    EXPECT_CALL(*m_directoryReaderMock, readDirectory(50))
        .WillOnce(Return(directoryResult({
            makeEntry("REPORT.PDF", fat32tree::DirectoryEntryType::File, 100, 123)})));
    EXPECT_CALL(*m_directoryReaderMock, readDirectory(20))
        .WillOnce(Return(directoryResult({
            makeEntry("PHOTO.JPG", fat32tree::DirectoryEntryType::File, 101, 456)})));

    const auto root = m_builder->buildTree();

    ASSERT_EQ(root.children.size(), 2U);
    ASSERT_EQ(root.children[0]->children.size(), 1U);
    ASSERT_EQ(root.children[1]->children.size(), 1U);
    EXPECT_EQ(root.children[0]->children[0]->name, "PHOTO.JPG");
    EXPECT_EQ(root.children[1]->children[0]->name, "REPORT.PDF");
}

TEST_F(FileTreeBuilderTest, KeepsEmptyDirectoryNode)
{
    EXPECT_CALL(*m_directoryReaderMock, readRootDirectory(2))
        .WillOnce(Return(rootResult({
            makeEntry("EMPTYDIR", fat32tree::DirectoryEntryType::Directory, 20)})));
    EXPECT_CALL(*m_directoryReaderMock, readDirectory(20))
        .WillOnce(Return(directoryResult({})));

    const auto root = m_builder->buildTree();

    ASSERT_EQ(root.children.size(), 1U);
    EXPECT_EQ(root.children[0]->name, "EMPTYDIR");
    EXPECT_EQ(root.children[0]->type, fat32tree::DirectoryEntryType::Directory);
    EXPECT_TRUE(root.children[0]->children.empty());
}

TEST_F(FileTreeBuilderTest, DoesNotReadFileClusterAsDirectory)
{
    EXPECT_CALL(*m_directoryReaderMock, readRootDirectory(2))
        .WillOnce(Return(rootResult({
            makeEntry("FILE.BIN", fat32tree::DirectoryEntryType::File, 100, 900)})));

    const auto root = m_builder->buildTree();

    ASSERT_EQ(root.children.size(), 1U);
    EXPECT_EQ(root.children[0]->name, "FILE.BIN");
}

TEST_F(FileTreeBuilderTest, DoesNotTraverseRootClusterAgain)
{
    EXPECT_CALL(*m_directoryReaderMock, readRootDirectory(2))
        .WillOnce(Return(rootResult({
            makeEntry("BACK_TO_ROOT", fat32tree::DirectoryEntryType::Directory, 2)})));

    const auto root = m_builder->buildTree();

    ASSERT_EQ(root.children.size(), 1U);
    EXPECT_EQ(root.children[0]->name, "BACK_TO_ROOT");
    EXPECT_TRUE(root.children[0]->children.empty());
}

TEST_F(FileTreeBuilderTest, DoesNotTraverseAlreadyVisitedDirectoryCluster)
{
    EXPECT_CALL(*m_directoryReaderMock, readRootDirectory(2))
        .WillOnce(Return(rootResult({
            makeEntry("A", fat32tree::DirectoryEntryType::Directory, 20),
            makeEntry("B", fat32tree::DirectoryEntryType::Directory, 20)})));
    EXPECT_CALL(*m_directoryReaderMock, readDirectory(20))
        .WillOnce(Return(directoryResult({
            makeEntry("CHILD", fat32tree::DirectoryEntryType::File)})));

    const auto root = m_builder->buildTree();

    ASSERT_EQ(root.children.size(), 2U);
    EXPECT_EQ(root.children[0]->children.size(), 1U);
    EXPECT_TRUE(root.children[1]->children.empty());
}

TEST_F(FileTreeBuilderTest, StopsLogicalDirectoryCycle)
{
    EXPECT_CALL(*m_directoryReaderMock, readRootDirectory(2))
        .WillOnce(Return(rootResult({
            makeEntry("A", fat32tree::DirectoryEntryType::Directory, 20)})));
    EXPECT_CALL(*m_directoryReaderMock, readDirectory(20))
        .WillOnce(Return(directoryResult({
            makeEntry("B", fat32tree::DirectoryEntryType::Directory, 40)})));
    EXPECT_CALL(*m_directoryReaderMock, readDirectory(40))
        .WillOnce(Return(directoryResult({
            makeEntry("A_AGAIN", fat32tree::DirectoryEntryType::Directory, 20)})));

    const auto root = m_builder->buildTree();

    ASSERT_EQ(root.children.size(), 1U);
    ASSERT_EQ(root.children[0]->children.size(), 1U);
    ASSERT_EQ(root.children[0]->children[0]->children.size(), 1U);
    EXPECT_EQ(root.children[0]->children[0]->children[0]->name, "A_AGAIN");
    EXPECT_TRUE(root.children[0]->children[0]->children[0]->children.empty());
}

TEST_F(FileTreeBuilderTest, PreservesNodesFromPartiallyReadDirectory)
{
    EXPECT_CALL(*m_directoryReaderMock, readRootDirectory(2))
        .WillOnce(Return(rootResult({
            makeEntry("PHOTOS", fat32tree::DirectoryEntryType::Directory, 20)})));
    EXPECT_CALL(*m_directoryReaderMock, readDirectory(20))
        .WillOnce(Return(directoryResult({
            makeEntry("FILE1", fat32tree::DirectoryEntryType::File),
            makeEntry("FILE2", fat32tree::DirectoryEntryType::File)},
            fat32tree::ClusterChainStatus::BadCluster)));

    const auto root = m_builder->buildTree();

    ASSERT_EQ(root.children.size(), 1U);
    ASSERT_EQ(root.children[0]->children.size(), 2U);
    EXPECT_EQ(root.children[0]->children[0]->name, "FILE1");
    EXPECT_EQ(root.children[0]->children[1]->name, "FILE2");
}

TEST_F(FileTreeBuilderTest, ContinuesBuildingOtherDirectoriesWhenOneIsEmptyOrBroken)
{
    EXPECT_CALL(*m_directoryReaderMock, readRootDirectory(2))
        .WillOnce(Return(rootResult({
            makeEntry("BROKEN", fat32tree::DirectoryEntryType::Directory, 20),
            makeEntry("GOOD", fat32tree::DirectoryEntryType::Directory, 30)})));
    EXPECT_CALL(*m_directoryReaderMock, readDirectory(30))
        .WillOnce(Return(directoryResult({
            makeEntry("OK.TXT", fat32tree::DirectoryEntryType::File)})));
    EXPECT_CALL(*m_directoryReaderMock, readDirectory(20))
        .WillOnce(Return(directoryResult({}, fat32tree::ClusterChainStatus::ReadError)));

    const auto root = m_builder->buildTree();

    ASSERT_EQ(root.children.size(), 2U);
    EXPECT_TRUE(root.children[0]->children.empty());
    ASSERT_EQ(root.children[1]->children.size(), 1U);
    EXPECT_EQ(root.children[1]->children[0]->name, "OK.TXT");
}

} // namespace
