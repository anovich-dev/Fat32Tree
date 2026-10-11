#pragma once

#include "fat32tree/boot/Fat32Bpb.h"
#include "fat32tree/boot/Fat32VolumeLayout.h"

#include <cstdint>
#include <stdexcept>
#include <string>

namespace fat32tree
{

enum class ValidationErrorCategory
{
    Malformed,
    Unsupported,
    Corrupted
};

class Fat32ValidationException : public std::runtime_error
{
public:
    Fat32ValidationException(ValidationErrorCategory category, const std::string& message)
        : std::runtime_error(message), m_category(category)
    {
    }

    ValidationErrorCategory category() const noexcept
    {
        return m_category;
    }

private:
    ValidationErrorCategory m_category;
};

class Fat32VolumeValidator
{
public:
    Fat32VolumeLayout validate(const Fat32Bpb& bpb, std::uint64_t imageSize) const;

private:
    void validateFat32Format(const Fat32Bpb& bpb) const;

    void validateSectorAndClusterGeometry(const Fat32Bpb& bpb, Fat32VolumeLayout& layout) const;

    void validateVolumeGeometry(
        const Fat32Bpb& bpb, std::uint64_t imageSize, Fat32VolumeLayout& layout) const;

    void validateFatGeometry(const Fat32Bpb& bpb, Fat32VolumeLayout& layout) const;

    void validateDataRegionGeometry(const Fat32Bpb& bpb, Fat32VolumeLayout& layout) const;

    void validateRootDirectoryGeometry(const Fat32Bpb& bpb, Fat32VolumeLayout& layout) const;

    std::uint64_t checkedMultiply(
        std::uint64_t lhs, std::uint64_t rhs, const char* operationName) const;

    std::uint64_t checkedAdd(std::uint64_t lhs, std::uint64_t rhs, const char* operationName) const;
};

} // namespace fat32tree
