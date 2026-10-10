#pragma once

#include "Fat32Constants.h"

#include <span>

namespace fat32tree
{

class IDirectoryEntryValidator
{
public:
    virtual ~IDirectoryEntryValidator() = default;

    virtual bool isValidShortName(std::span<const std::uint8_t, fat32::kShortNameLength> rawName) const = 0;
};

}// namespace fat32tree