#pragma once

#include <concepts>
#include <cstdint>

namespace fat32tree
{

enum class ReadStatus
{
    Success,         // The requested bytes were read successfully.
    OutOfBounds,     // The requested byte range lies outside the image.
    InvalidReadSize, // The requested size cannot be represented by the stream API.
    SeekFailed,      // Failed to move the stream to the requested offset.
    ShortRead,       // Fewer bytes were read than requested.
    ReadFailed       // A low-level I/O error occurred while reading.
};

template <typename T>
concept SupportedReadType =
    std::same_as<T, std::uint8_t> ||
    std::same_as<T, std::uint16_t> ||
    std::same_as<T, std::uint32_t>;

template <SupportedReadType T>
struct ReadResult
{
    ReadStatus status;
    T value{};
};

constexpr const char* readStatusToString(ReadStatus status) noexcept
{
    switch (status)
    {
        case ReadStatus::Success:
            return "Success";

        case ReadStatus::OutOfBounds:
            return "OutOfBounds";

        case ReadStatus::InvalidReadSize:
            return "InvalidReadSize";

        case ReadStatus::SeekFailed:
            return "SeekFailed";

        case ReadStatus::ShortRead:
            return "ShortRead";

        case ReadStatus::ReadFailed:
            return "ReadFailed";
    }

    return "Unknown";
}

} // namespace fat32tree