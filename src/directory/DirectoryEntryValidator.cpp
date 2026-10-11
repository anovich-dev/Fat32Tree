#include "fat32tree/directory/DirectoryEntryValidator.h"

namespace fat32tree
{

static constexpr std::uint8_t kSpaceCharacter = 0x20;

bool DirectoryEntryValidator::isValidShortName(
    std::span<const std::uint8_t, fat32::kShortNameLength> rawName) const
{
    if (isDotEntry(rawName) || isDotDotEntry(rawName))
    {
        return true;
    }

    // A normal short name cannot start with a space.
    if (rawName[0] == kSpaceCharacter)
    {
        return false;
    }

    for (std::size_t i = 0; i < rawName.size(); ++i)
    {
        const std::uint8_t byte = rawName[i];

        // Space is valid and is also used for trailing padding.
        if (byte == kSpaceCharacter)
        {
            continue;
        }

        // 0x05 is valid only as the special first-byte Kanji marker.
        if (byte == fat32::kDirectoryEntryKanjiMarker)
        {
            if (i == 0)
            {
                continue;
            }

            return false;
        }

        if (isForbiddenCharacter(byte))
        {
            return false;
        }

        // FAT short names stored in DIR_Name are uppercase.
        if (byte >= 'a' && byte <= 'z')
        {
            return false;
        }
    }

    return true;
}

bool DirectoryEntryValidator::isDotEntry(
    std::span<const std::uint8_t, fat32::kShortNameLength> rawName) const
{
    if (rawName[0] != '.')
    {
        return false;
    }

    for (std::size_t i = 1; i < rawName.size(); ++i)
    {
        if (rawName[i] != kSpaceCharacter)
        {
            return false;
        }
    }

    return true;
}

bool DirectoryEntryValidator::isDotDotEntry(
    std::span<const std::uint8_t, fat32::kShortNameLength> rawName) const
{
    if (rawName[0] != '.' || rawName[1] != '.')
    {
        return false;
    }

    for (std::size_t i = 2; i < rawName.size(); ++i)
    {
        if (rawName[i] != kSpaceCharacter)
        {
            return false;
        }
    }

    return true;
}

bool DirectoryEntryValidator::isForbiddenCharacter(std::uint8_t byte) const
{
    // Values below 0x20 are control characters in the FAT short-name field.
    if (byte < kSpaceCharacter)
    {
        return true;
    }

    switch (byte)
    {
    case 0x22: // "
    case 0x2A: // *
    case 0x2B: // +
    case 0x2C: // ,
    case 0x2E: // .
    case 0x2F: // /
    case 0x3A: // :
    case 0x3B: // ;
    case 0x3C: // <
    case 0x3D: // =
    case 0x3E: // >
    case 0x3F: // ?
    case 0x5B: // [
    case 0x5C: // backslash
    case 0x5D: // ]
    case 0x7C: // |
        return true;

    default:
        return false;
    }
}

} // namespace fat32tree
