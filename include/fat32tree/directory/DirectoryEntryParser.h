#pragma once

#include "fat32tree/directory/IDirectoryEntryParser.h"

#include "fat32tree/core/Fat32Constants.h"
#include "fat32tree/directory/DirectoryEntry.h"
#include "fat32tree/directory/IDirectoryEntryValidator.h"

#include <cstdint>
#include <memory>
#include <span>
#include <string>

namespace fat32tree
{

class BinaryReader;

class DirectoryEntryParser final : public IDirectoryEntryParser
{
public:
    DirectoryEntryParser(BinaryReader& reader, std::unique_ptr<IDirectoryEntryValidator> validator);

    ParsedDirectoryEntry parse(std::uint64_t entryOffset) const override;

private:
    std::string parseVolumeLabel(
        std::span<const std::uint8_t, fat32::kShortNameLength> rawName) const;

    std::string parseShortName(
        std::span<const std::uint8_t, fat32::kShortNameLength> rawName) const;

    std::string parseNamePart(std::span<const std::uint8_t> bytes) const;

    std::uint32_t combineFirstCluster(std::uint16_t high, std::uint16_t low) const;

private:
    BinaryReader& m_reader;
    std::unique_ptr<IDirectoryEntryValidator> m_validator;
};

} // namespace fat32tree
