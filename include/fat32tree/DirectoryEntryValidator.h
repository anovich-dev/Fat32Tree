#pragma once

#include "IDirectoryEntryValidator.h"

#include "fat32tree/Fat32Constants.h"

#include <cstdint>
#include <span>

namespace fat32tree
{

class DirectoryEntryValidator final : public IDirectoryEntryValidator
{
public:
    bool isValidShortName(std::span<const std::uint8_t, fat32::kShortNameLength> rawName) const;

private:
    bool isDotEntry(std::span<const std::uint8_t, fat32::kShortNameLength> rawName) const;

    bool isDotDotEntry(std::span<const std::uint8_t, fat32::kShortNameLength> rawName) const;

    bool isForbiddenCharacter(std::uint8_t byte) const;
};

}// namespace fat32tree