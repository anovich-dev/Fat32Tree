#pragma once

#include "Fat32VolumeLayout.h"
#include "FileNode.h"

#include <memory>
#include <unordered_set>
#include <stack>

namespace fat32tree
{

class IDirectoryReader;

class FileTreeBuilder
{
public:
    FileTreeBuilder(const Fat32VolumeLayout &layout,
                    std::unique_ptr<IDirectoryReader> directoryReader);

    FileNode buildTree();

private:

    struct DirectoryWorkItem
    {
        FileNode* node;
        std::uint32_t firstCluster;
    };

    void addEntries(
        FileNode &parent,
        const std::vector<DirectoryEntry> &entries,
        std::unordered_set<std::uint32_t> &visitedDirectories,
        std::stack<DirectoryWorkItem> &directoryItemsStack);

private:
    const Fat32VolumeLayout& m_layout;
    std::unique_ptr<IDirectoryReader> m_directoryReader;
};

} // namespace fat32tree