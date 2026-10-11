#include "fat32tree/tree/FileTreeBuilder.h"

#include "fat32tree/boot/Fat32VolumeLayout.h"
#include "fat32tree/core/Logger.h"
#include "fat32tree/directory/DirectoryReader.h"

#include <utility>
#include <stack>
#include <memory>
#include <unordered_set>

namespace fat32tree
{

FileTreeBuilder::FileTreeBuilder(
    const Fat32VolumeLayout& layout,
    std::unique_ptr<IDirectoryReader> directoryReader)
    : m_layout(layout),
      m_directoryReader(std::move(directoryReader))
{
}

FileNode FileTreeBuilder::buildTree()
{
    const auto rootResult = m_directoryReader->readRootDirectory(m_layout.rootCluster);

    const std::string rootName = rootResult.volumeLabel.value_or("/");

    FileNode root{
        rootName,
        DirectoryEntryType::Directory,
        m_layout.rootCluster,
        0,
        {}
    };

    std::unordered_set<std::uint32_t> visitedDirectories;
    visitedDirectories.insert(m_layout.rootCluster);

    std::stack<DirectoryWorkItem> directoryItemsStack;

    addEntries(
        root,
        rootResult.entries,
        visitedDirectories,
        directoryItemsStack);

    while (!directoryItemsStack.empty())
    {
        const DirectoryWorkItem directoryItem = directoryItemsStack.top();

        directoryItemsStack.pop();

        const auto directoryResult = m_directoryReader->readDirectory(directoryItem.firstCluster);

        addEntries(
            *directoryItem.node,
            directoryResult.entries,
            visitedDirectories,
            directoryItemsStack);
    }

    return root;
}

void FileTreeBuilder::addEntries(
    FileNode& parent,
    const std::vector<DirectoryEntry>& entries,
    std::unordered_set<std::uint32_t>& visitedDirectories,
    std::stack<DirectoryWorkItem>& directoryItemsStack)
{
    for (const auto& entry : entries)
    {
        auto child = std::make_unique<FileNode>(
            FileNode{
                entry.name,
                entry.type,
                entry.firstCluster,
                entry.fileSize,
                {}
            });

        FileNode* childPtr = child.get();

        parent.children.push_back(std::move(child));

        if (childPtr->type == DirectoryEntryType::Directory)
        {
            if (visitedDirectories.contains(childPtr->firstCluster))
            {
                Logger::warning("Directory '" + childPtr->name +
                "' points to already visited cluster " +
                std::to_string(childPtr->firstCluster) + "; skipping repeated traversal");
            }
            else
            {
                visitedDirectories.insert(
                childPtr->firstCluster);

            directoryItemsStack.push(
                DirectoryWorkItem{
                    childPtr,
                    childPtr->firstCluster
                });
            }
        }
    }
}

} // namespace fat32tree
