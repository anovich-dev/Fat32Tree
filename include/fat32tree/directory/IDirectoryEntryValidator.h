#include "fat32tree/core/Fat32Constants.h"

#include <cstdint>
#include <span>

#pragma once

namespace fat32tree
{

class IDirectoryEntryValidator
{
public:
    virtual ~IDirectoryEntryValidator() = default;

    virtual bool isValidShortName(
        std::span<const std::uint8_t, fat32::kShortNameLength> rawName) const = 0;
};

} // namespace fat32tree
