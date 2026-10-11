#include "fat32tree/BinaryReader.h"
#include "fat32tree/BootSectorParser.h"
#include "fat32tree/ClusterChainReader.h"
#include "fat32tree/DirectoryEntryParser.h"
#include "fat32tree/DirectoryEntryValidator.h"
#include "fat32tree/DirectoryReader.h"
#include "fat32tree/Fat32VolumeValidator.h"
#include "fat32tree/FatTableReader.h"
#include "fat32tree/FileTreeBuilder.h"
#include "fat32tree/TreePrinter.h"

#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: fat32tree <image-path>\n";
        return 1;
    }

    try
    {
        const std::filesystem::path imagePath{argv[1]};

        // 1. Input / volume
        fat32tree::BinaryReader binaryReader{imagePath};

        fat32tree::BootSectorParser bootSectorParser{binaryReader};
        const fat32tree::Fat32Bpb bpb = bootSectorParser.parse();

        fat32tree::Fat32VolumeValidator volumeValidator;
        const fat32tree::Fat32VolumeLayout layout =
            volumeValidator.validate(
                bpb,
                binaryReader.size());

        // 2. FAT
        fat32tree::FatTableReader fatTableReader{
            binaryReader,
            layout
        };

        auto clusterChainReader =
            std::make_unique<fat32tree::ClusterChainReader>(
                fatTableReader,
                layout);

        // 3. Directory
        auto directoryEntryValidator =
            std::make_unique<fat32tree::DirectoryEntryValidator>();

        auto directoryEntryParser =
            std::make_unique<fat32tree::DirectoryEntryParser>(
                binaryReader,
                std::move(directoryEntryValidator));

        auto directoryReader =
            std::make_unique<fat32tree::DirectoryReader>(
                layout,
                std::move(clusterChainReader),
                std::move(directoryEntryParser));

        // 4. Tree / output
        fat32tree::FileTreeBuilder treeBuilder{
            layout,
            std::move(directoryReader)
        };

        const fat32tree::FileNode root = treeBuilder.buildTree();

        fat32tree::TreePrinter treePrinter;
        treePrinter.print(root, std::cout);

        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr
            << "Error: "
            << exception.what()
            << '\n';

        return 1;
    }
}