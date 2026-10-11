#pragma once

#include "fat32tree/directory/IDirectoryReader.h"
#include "fat32tree/tree/FileNode.h"

#include <cstdint>
#include <memory>
#include <stack>
#include <unordered_set>
#include <vector>

namespace fat32tree
{

struct Fat32VolumeLayout;

class FileTreeBuilder
{
public:
    FileTreeBuilder(
        const Fat32VolumeLayout& layout, std::unique_ptr<IDirectoryReader> directoryReader);

    FileNode buildTree();

private:
    struct DirectoryWorkItem
    {
        FileNode* node;
        std::uint32_t firstCluster;
    };

    void addEntries(FileNode& parent, const std::vector<DirectoryEntry>& entries,
        std::unordered_set<std::uint32_t>& visitedDirectories,
        std::stack<DirectoryWorkItem>& directoryItemsStack);

private:
    const Fat32VolumeLayout& m_layout;
    std::unique_ptr<IDirectoryReader> m_directoryReader;
};

} // namespace fat32tree
