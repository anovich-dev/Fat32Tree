#pragma once

#include "fat32tree/fat/FatEntry.h"
#include "fat32tree/fat/FatEntryParser.h"

#include <cstdint>
#include <stdexcept>
#include <string>

namespace fat32tree
{

class BinaryReader;
struct Fat32VolumeLayout;

class FatTableReadException : public std::runtime_error
{
public:
    explicit FatTableReadException(const std::string& message) : std::runtime_error(message)
    {
    }
};

class FatTableReader
{
public:
    FatTableReader(BinaryReader& reader, const Fat32VolumeLayout& layout);

    FatEntry getEntry(std::uint32_t clusterNumber) const;

private:
    BinaryReader& m_reader;
    const Fat32VolumeLayout& m_layout;
    FatEntryParser m_entryParser;
};

} // namespace fat32tree
