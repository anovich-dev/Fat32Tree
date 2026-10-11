#pragma once

#include "IDirectoryEntryParser.h"

#include "DirectoryEntry.h"
#include "BinaryReader.h"
#include "Fat32Constants.h"

#include <cstdint>
#include <span>

namespace fat32tree
{

class IDirectoryEntryValidator;

class DirectoryEntryParser final : public IDirectoryEntryParser
{
public:
    DirectoryEntryParser(BinaryReader& reader, std::unique_ptr<IDirectoryEntryValidator> validator);

    ParsedDirectoryEntry parse(std::uint64_t entryOffset) const override;

private:
    
    std::string parseVolumeLabel(std::span<const std::uint8_t, fat32::kShortNameLength> rawName) const;

    std::string parseShortName(std::span<const std::uint8_t, fat32::kShortNameLength> rawName) const;

    std::string parseNamePart(std::span<const std::uint8_t> bytes) const;

    std::uint32_t combineFirstCluster(std::uint16_t high, std::uint16_t low) const;

private:
    BinaryReader& m_reader;
    std::unique_ptr<IDirectoryEntryValidator> m_validator;
};

} // namespace fat32tree