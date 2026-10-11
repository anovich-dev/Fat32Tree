#include "fat32tree/BinaryReader.h"
#include "fat32tree/BootSectorParser.h"
#include "fat32tree/ClusterChainReader.h"
#include "fat32tree/DirectoryEntryParser.h"
#include "fat32tree/DirectoryEntryValidator.h"
#include "fat32tree/DirectoryReader.h"
#include "fat32tree/Fat32VolumeLayout.h"
#include "fat32tree/Fat32VolumeValidator.h"
#include "fat32tree/FatTableReader.h"
#include "fat32tree/FileNode.h"
#include "fat32tree/FileTreeBuilder.h"
#include "fat32tree/Logger.h"
#include "fat32tree/TreePrinter.h"

#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <sstream>
#include <utility>

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: fat32tree <image-path>\n";
        return 1;
    }

    try
    {
        fat32tree::Logger::initialize();
    }
    catch (const std::exception& exception)
    {
        // Initialization failures cannot be reported through the logger.
        std::cerr << "Error: " << exception.what() << '\n';
        return 1;
    }

    try
    {
        fat32tree::Logger::info("Fat32Tree started");
        const std::filesystem::path imagePath{argv[1]};
        fat32tree::Logger::info("Input image: " + imagePath.string());

        // 1. Input / volume
        fat32tree::BinaryReader binaryReader{imagePath};

        fat32tree::BootSectorParser bootSectorParser{binaryReader};
        const fat32tree::Fat32Bpb bpb = bootSectorParser.parse();

        std::ostringstream metadata;
        metadata << "Parsed BPB:\n    bytesPerSector=" << bpb.bytesPerSector
                 << "\n    sectorsPerCluster=" << static_cast<unsigned>(bpb.sectorsPerCluster)
                 << "\n    reservedSectorCount=" << bpb.reservedSectorCount
                 << "\n    fatCount=" << static_cast<unsigned>(bpb.fatCount)
                 << "\n    rootEntryCount=" << bpb.rootEntryCount
                 << "\n    totalSectors16=" << bpb.totalSectors16
                 << "\n    totalSectors32=" << bpb.totalSectors32
                 << "\n    fatSize16=" << bpb.fatSize16
                 << "\n    fatSize32=" << bpb.fatSize32
                 << "\n    extendedFlags=" << bpb.extendedFlags
                 << "\n    fileSystemVersion=" << bpb.fileSystemVersion
                 << "\n    rootCluster=" << bpb.rootCluster
                 << "\n    fsInfoSector=" << bpb.fsInfoSector
                 << "\n    backupBootSector=" << bpb.backupBootSector
                 << "\n    signatureFirstByte=" << static_cast<unsigned>(bpb.signatureFirstByte)
                 << "\n    signatureSecondByte=" << static_cast<unsigned>(bpb.signatureSecondByte);
        fat32tree::Logger::debug(metadata.str());

        fat32tree::Fat32VolumeValidator volumeValidator;
        const fat32tree::Fat32VolumeLayout layout =
            volumeValidator.validate(bpb, binaryReader.size());
        fat32tree::Logger::info("FAT32 volume successfully validated");

        // 2. FAT
        fat32tree::FatTableReader fatTableReader{binaryReader, layout};

        auto clusterChainReader =
            std::make_unique<fat32tree::ClusterChainReader>(
                fatTableReader,
                layout);

        // 3. Directory
        auto directoryEntryValidator = std::make_unique<fat32tree::DirectoryEntryValidator>();

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
        fat32tree::Logger::info("File tree successfully built");

        fat32tree::TreePrinter treePrinter;
        treePrinter.print(root, std::cout);

        fat32tree::Logger::info("Fat32Tree finished successfully");
        return 0;
    }
    catch (const std::exception& exception)
    {
        fat32tree::Logger::error(exception.what());
        std::cerr
            << "Error: "
            << exception.what()
            << '\n';

        return 1;
    }
}
