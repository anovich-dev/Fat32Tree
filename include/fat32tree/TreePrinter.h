#pragma once

#include <iosfwd>
#include <stack>
#include <string>

namespace fat32tree
{

struct FileNode;

class TreePrinter
{
public:
    void print(const FileNode& root, std::ostream& output) const;

private:
    struct PrintWorkItem
    {
        const FileNode* node;
        std::string prefix;
        bool isLast;
    };

    void pushChildren(const FileNode& parent, const std::string& prefix,
        std::stack<PrintWorkItem>& workItems) const;
};

} // namespace fat32tree
