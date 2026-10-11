#include "fat32tree/BinaryReader.h"

#include <array>
#include <cstddef>
#include <ios>
#include <limits>
#include <span>

namespace fat32tree
{

BinaryReader::BinaryReader(const std::filesystem::path& path) : m_stream(path, std::ios::binary)
{
    if (!m_stream.is_open())
    {
        throw BinaryReaderException("Failed to open image file");
    }

    m_stream.seekg(0, std::ios::end);

    if (!m_stream)
    {
        throw BinaryReaderException("Failed to determine image file size");
    }

    const auto endPosition = m_stream.tellg();

    if (endPosition < 0)
    {
        throw BinaryReaderException("Failed to determine image file size");
    }

    m_size = static_cast<std::uint64_t>(endPosition);

    if (m_size > static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max()))
    {
        throw BinaryReaderException("Image file is too large for stream addressing");
    }

    m_stream.seekg(0, std::ios::beg);

    if (!m_stream)
    {
        throw BinaryReaderException("Failed to reset image stream position");
    }
}

std::uint64_t BinaryReader::size() const noexcept
{
    return m_size;
}

ReadResult<std::uint8_t> BinaryReader::readU8(std::uint64_t offset)
{
    std::byte byte{};

    const ReadStatus status = readExact(offset, std::span<std::byte>(&byte, 1));

    if (status != ReadStatus::Success)
    {
        return {status, 0};
    }

    return {ReadStatus::Success, std::to_integer<std::uint8_t>(byte)};
}

ReadResult<std::uint16_t> BinaryReader::readU16LE(std::uint64_t offset)
{
    std::array<std::byte, 2> bytes{};

    const ReadStatus status = readExact(offset, std::span<std::byte>(bytes));

    if (status != ReadStatus::Success)
    {
        return {status, 0};
    }

    const auto lowByte = std::to_integer<std::uint16_t>(bytes[0]);
    const auto highByte = std::to_integer<std::uint16_t>(bytes[1]);

    const std::uint16_t value = lowByte | (highByte << 8);

    return {ReadStatus::Success, value};
}

ReadResult<std::uint32_t> BinaryReader::readU32LE(std::uint64_t offset)
{
    std::array<std::byte, 4> bytes{};

    const ReadStatus status = readExact(offset, std::span<std::byte>(bytes));

    if (status != ReadStatus::Success)
    {
        return {status, 0};
    }

    const std::uint32_t value = std::to_integer<std::uint32_t>(bytes[0]) |
                                (std::to_integer<std::uint32_t>(bytes[1]) << 8) |
                                (std::to_integer<std::uint32_t>(bytes[2]) << 16) |
                                (std::to_integer<std::uint32_t>(bytes[3]) << 24);

    return {ReadStatus::Success, value};
}

ReadStatus BinaryReader::readBytes(std::uint64_t offset, std::span<std::uint8_t> buffer)
{
    return readExact(offset, std::as_writable_bytes(buffer));
}

ReadStatus BinaryReader::readExact(std::uint64_t offset, std::span<std::byte> buffer)
{
    if (offset > m_size || buffer.size() > m_size - offset)
    {
        return ReadStatus::OutOfBounds;
    }

    if (buffer.empty())
    {
        return ReadStatus::Success;
    }

    if (buffer.size() > static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max()))
    {
        return ReadStatus::InvalidReadSize;
    }

    // A previous failed operation may have left fail/eof state flags set.
    // Its error has already been reported by the previous readExact() call.
    if (!m_stream.good())
    {
        m_stream.clear();
    }

    m_stream.seekg(static_cast<std::streamoff>(offset), std::ios::beg);

    if (!m_stream)
    {
        return ReadStatus::SeekFailed;
    }

    const auto requestedSize = static_cast<std::streamsize>(buffer.size());

    m_stream.read(reinterpret_cast<char*>(buffer.data()), requestedSize);

    if (m_stream.bad())
    {
        return ReadStatus::ReadFailed;
    }

    if (m_stream.gcount() != requestedSize)
    {
        return ReadStatus::ShortRead;
    }

    return ReadStatus::Success;
}

} // namespace fat32tree
