#pragma once

#include "fat32tree/ReadResult.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <stdexcept>
#include <string>

namespace fat32tree
{

class BinaryReaderException : public std::runtime_error
{
public:
    explicit BinaryReaderException(const std::string& message) : std::runtime_error(message)
    {
    }
};

class BinaryReader
{
public:
    explicit BinaryReader(const std::filesystem::path& path);

    std::uint64_t size() const noexcept;
    ReadResult<std::uint8_t> readU8(std::uint64_t offset);
    ReadResult<std::uint16_t> readU16LE(std::uint64_t offset);
    ReadResult<std::uint32_t> readU32LE(std::uint64_t offset);

    ReadStatus readBytes(std::uint64_t offset, std::span<std::uint8_t> buffer);

private:
    ReadStatus readExact(std::uint64_t offset, std::span<std::byte> buffer);

private:
    std::ifstream m_stream;
    std::uint64_t m_size = 0;
};

} // namespace fat32tree
