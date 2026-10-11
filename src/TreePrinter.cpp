#include "fat32tree/TreePrinter.h"

#include <stack>
#include <utility>

namespace fat32tree
{

void TreePrinter::print(const FileNode& root, std::ostream& output) const
{
    output << root.name;

    // If root uses a volume label as its name, append '/' for directory display.
    // By default, the root name is "/", so no extra separator is needed.
    if (root.type == DirectoryEntryType::Directory &&
        root.name != "/")
    {
        output << '/';
    }

    output << '\n';

    std::stack<PrintWorkItem> workItems;

    pushChildren(root, "", workItems);

    while (!workItems.empty())
    {
        PrintWorkItem workItem = std::move(workItems.top());

        workItems.pop();

        const FileNode& node = *workItem.node;

        output << workItem.prefix
               << (workItem.isLast ? "└── " : "├── ")
               << node.name;

        if (node.type == DirectoryEntryType::Directory)
        {
            output << '/';
        }

        output << '\n';

        const std::string childPrefix =
            workItem.prefix +
            (workItem.isLast ? "    " : "│   ");

        pushChildren(node, childPrefix, workItems);
    }
}

void TreePrinter::pushChildren(
        const FileNode& parent,
        const std::string& prefix,
        std::stack<PrintWorkItem>& workItems) const
{
    // Push children in reverse order because std::stack is LIFO,
    // so they are printed in their original order.
    for (std::size_t i = parent.children.size(); i > 0; --i)
    {
        const std::size_t childIndex = i - 1;

        workItems.push(
            PrintWorkItem{
                parent.children[childIndex].get(),
                prefix,
                childIndex == parent.children.size() - 1
            });
    }    
}

} // namespace fat32tree
