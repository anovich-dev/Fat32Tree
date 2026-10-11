#include "fat32tree/DirectoryEntryParser.h"

#include "fat32tree/BinaryReader.h"

#include <array>
#include <utility>

namespace fat32tree
{

DirectoryEntryParser::DirectoryEntryParser(
    BinaryReader& reader, std::unique_ptr<IDirectoryEntryValidator> validator)
    : m_reader(reader), m_validator(std::move(validator))
{
}

ParsedDirectoryEntry DirectoryEntryParser::parse(std::uint64_t entryOffset) const
{
    std::array<std::uint8_t, fat32::kShortNameLength> rawName{};

    const ReadStatus nameStatus =
        m_reader.readBytes(entryOffset + fat32::kDirectoryNameOffset, rawName);

    if (nameStatus != ReadStatus::Success)
    {
        return {DirectoryEntryState::Invalid, {}};
    }

    // DIR_Name[0] == 0x00:
    // this entry and all following entries are free.
    if (rawName[0] == fat32::kDirectoryEntryEndMarker)
    {
        return {DirectoryEntryState::End, {}};
    }

    // DIR_Name[0] == 0xE5:
    // this entry is deleted/free.
    if (rawName[0] == fat32::kDirectoryEntryDeletedMarker)
    {
        return {DirectoryEntryState::Deleted, {}};
    }

    const auto attributeResult = m_reader.readU8(entryOffset + fat32::kDirectoryAttributeOffset);

    if (attributeResult.status != ReadStatus::Success)
    {
        return {DirectoryEntryState::Invalid, {}};
    }

    const std::uint8_t attributes = attributeResult.value;

    // Must be checked before VolumeLabel because ATTR_LONG_NAME
    // contains the ATTR_VOLUME_ID bit.
    if (attributes == fat32::kAttributeLongName)
    {
        return {DirectoryEntryState::LongFileName, {}};
    }

    if ((attributes & fat32::kAttributeVolumeId) != 0)
    {
        const std::string volumeName = parseVolumeLabel(rawName);

        DirectoryEntry entry{
            volumeName,
            DirectoryEntryType::Directory,
            0,
            0
        };

        return {DirectoryEntryState::VolumeLabel, entry};
    }

    if (!m_validator->isValidShortName(rawName))
    {
        return {DirectoryEntryState::Invalid, {}};
    }

    const std::string parsedShortName = parseShortName(rawName);

    const auto highResult =
        m_reader.readU16LE(entryOffset + fat32::kDirectoryFirstClusterHighOffset);

    const auto lowResult = m_reader.readU16LE(entryOffset + fat32::kDirectoryFirstClusterLowOffset);

    if (highResult.status != ReadStatus::Success || lowResult.status != ReadStatus::Success)
    {
        return {DirectoryEntryState::Invalid, {}};
    }

    const std::uint32_t firstCluster = combineFirstCluster(highResult.value, lowResult.value);

    const auto fileSizeResult = m_reader.readU32LE(entryOffset + fat32::kDirectoryFileSizeOffset);

    if (fileSizeResult.status != ReadStatus::Success)
    {
        return {DirectoryEntryState::Invalid, {}};
    }

    const DirectoryEntryType entryType =
        (attributes & fat32::kAttributeDirectory) != 0
                                             ? DirectoryEntryType::Directory
                                             : DirectoryEntryType::File;

    DirectoryEntry entry{
        parsedShortName,
        entryType,
        firstCluster,
        fileSizeResult.value
    };

    return {DirectoryEntryState::Normal, entry};
}

std::string DirectoryEntryParser::parseVolumeLabel(
    std::span<const std::uint8_t, fat32::kShortNameLength> rawName) const
{
    return parseNamePart(rawName);
}

std::string DirectoryEntryParser::parseShortName(
    std::span<const std::uint8_t, fat32::kShortNameLength> rawName) const
{
    const std::string baseName = parseNamePart(rawName.first<fat32::kShortNameBaseLength>());

    const std::string extension = parseNamePart(
        rawName.subspan<fat32::kShortNameBaseLength, fat32::kShortNameExtensionLength>());

    if (extension.empty())
    {
        return baseName;
    }

    return baseName + "." + extension;
}

std::string DirectoryEntryParser::parseNamePart(std::span<const std::uint8_t> bytes) const
{
    std::string result;
    result.reserve(bytes.size());

    for (const std::uint8_t byte : bytes)
    {
        result.push_back(static_cast<char>(byte));
    }

    while (!result.empty() && result.back() == ' ')
    {
        result.pop_back();
    }

    return result;
}

std::uint32_t DirectoryEntryParser::combineFirstCluster(std::uint16_t high, std::uint16_t low) const
{
    return
        (static_cast<std::uint32_t>(high) << 16) |
        static_cast<std::uint32_t>(low);
}

} // namespace fat32tree
