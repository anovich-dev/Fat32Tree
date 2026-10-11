#pragma once

#include "fat32tree/fat/FatEntry.h"

#include <cstdint>

namespace fat32tree
{

class FatEntryParser
{
public:
    FatEntry parse(std::uint32_t rawValue) const;
};

} // namespace fat32tree
