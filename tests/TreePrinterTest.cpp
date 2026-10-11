#include <fat32tree/TreePrinter.h>

#include <gtest/gtest.h>

#include <memory>
#include <sstream>
#include <string>
#include <utility>

namespace
{

using fat32tree::DirectoryEntryType;
using fat32tree::FileNode;
using fat32tree::TreePrinter;

FileNode makeDirectory(std::string name)
{
    return {std::move(name), DirectoryEntryType::Directory, 0, 0, {}};
}

FileNode& addChild(FileNode& parent, std::string name, DirectoryEntryType type)
{
    parent.children.push_back(std::make_unique<FileNode>(
        FileNode{std::move(name), type, 0, 0, {}}));
    return *parent.children.back();
}

FileNode& addDirectory(FileNode& parent, std::string name)
{
    return addChild(parent, std::move(name), DirectoryEntryType::Directory);
}

void addFile(FileNode& parent, std::string name)
{
    addChild(parent, std::move(name), DirectoryEntryType::File);
}

TEST(TreePrinterTest, PrintsRootWithoutVolumeLabel)
{
    const auto root = makeDirectory("/");
    std::ostringstream output;
    TreePrinter printer;

    printer.print(root, output);

    EXPECT_EQ(output.str(), "/\n");
}

TEST(TreePrinterTest, PrintsRootWithVolumeLabel)
{
    const auto root = makeDirectory("MY_USB");
    std::ostringstream output;
    TreePrinter printer;

    printer.print(root, output);

    EXPECT_EQ(output.str(), "MY_USB/\n");
}

TEST(TreePrinterTest, PrintsSingleFile)
{
    auto root = makeDirectory("/");
    addFile(root, "FILE.TXT");
    std::ostringstream output;
    TreePrinter printer;

    printer.print(root, output);

    EXPECT_EQ(output.str(),
        "/\n"
        "└── FILE.TXT\n");
}

TEST(TreePrinterTest, PrintsSingleDirectory)
{
    auto root = makeDirectory("/");
    addDirectory(root, "DOCS");
    std::ostringstream output;
    TreePrinter printer;

    printer.print(root, output);

    EXPECT_EQ(output.str(),
        "/\n"
        "└── DOCS/\n");
}

TEST(TreePrinterTest, PrintsMultipleRootChildren)
{
    auto root = makeDirectory("/");
    addFile(root, "A.TXT");
    addFile(root, "B.TXT");
    addDirectory(root, "DIR");
    std::ostringstream output;
    TreePrinter printer;

    printer.print(root, output);

    EXPECT_EQ(output.str(),
        "/\n"
        "├── A.TXT\n"
        "├── B.TXT\n"
        "└── DIR/\n");
}

TEST(TreePrinterTest, PrintsNestedDirectory)
{
    auto root = makeDirectory("/");
    auto& directory = addDirectory(root, "DIR");
    addFile(directory, "FILE.TXT");
    std::ostringstream output;
    TreePrinter printer;

    printer.print(root, output);

    EXPECT_EQ(output.str(),
        "/\n"
        "└── DIR/\n"
        "    └── FILE.TXT\n");
}

TEST(TreePrinterTest, PrintsMultipleLevelsWithCorrectPrefixes)
{
    auto root = makeDirectory("MY_USB");
    addFile(root, "README.TXT");
    auto& photos = addDirectory(root, "PHOTOS");
    addFile(photos, "A.JPG");
    auto& trip = addDirectory(photos, "TRIP");
    addFile(trip, "B.JPG");
    addDirectory(root, "EMPTYDIR");
    std::ostringstream output;
    TreePrinter printer;

    printer.print(root, output);

    EXPECT_EQ(output.str(),
        "MY_USB/\n"
        "├── README.TXT\n"
        "├── PHOTOS/\n"
        "│   ├── A.JPG\n"
        "│   └── TRIP/\n"
        "│       └── B.JPG\n"
        "└── EMPTYDIR/\n");
}

TEST(TreePrinterTest, PreservesChildrenOrder)
{
    auto root = makeDirectory("/");
    addFile(root, "FIRST.TXT");
    addFile(root, "SECOND.TXT");
    addFile(root, "THIRD.TXT");
    std::ostringstream output;
    TreePrinter printer;

    printer.print(root, output);

    EXPECT_EQ(output.str(),
        "/\n"
        "├── FIRST.TXT\n"
        "├── SECOND.TXT\n"
        "└── THIRD.TXT\n");
}

TEST(TreePrinterTest, PrintsEmptyDirectory)
{
    auto root = makeDirectory("/");
    const auto& empty = addDirectory(root, "EMPTY");
    ASSERT_TRUE(empty.children.empty());
    std::ostringstream output;
    TreePrinter printer;

    printer.print(root, output);

    EXPECT_EQ(output.str(),
        "/\n"
        "└── EMPTY/\n");
}

TEST(TreePrinterTest, PrintsSiblingDirectoriesWithDifferentDepths)
{
    auto root = makeDirectory("/");
    auto& a = addDirectory(root, "A");
    auto& a1 = addDirectory(a, "A1");
    addFile(a1, "FILE.TXT");
    auto& b = addDirectory(root, "B");
    addFile(b, "B.TXT");
    std::ostringstream output;
    TreePrinter printer;

    printer.print(root, output);

    EXPECT_EQ(output.str(),
        "/\n"
        "├── A/\n"
        "│   └── A1/\n"
        "│       └── FILE.TXT\n"
        "└── B/\n"
        "    └── B.TXT\n");
}

} // namespace
