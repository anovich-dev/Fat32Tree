#include "fat32tree/Fat32VolumeValidator.h"

#include "fat32tree/Fat32Constants.h"
#include "fat32tree/Fat32VolumeLayout.h"

#include <limits>
#include <string>

namespace fat32tree
{

Fat32VolumeLayout Fat32VolumeValidator::validate(const Fat32Bpb& bpb, std::uint64_t imageSize) const
{
    Fat32VolumeLayout layout;

    validateFat32Format(bpb);

    validateSectorAndClusterGeometry(bpb, layout);

    validateVolumeGeometry(bpb, imageSize, layout);

    validateFatGeometry(bpb, layout);

    validateDataRegionGeometry(bpb, layout);

    validateRootDirectoryGeometry(bpb, layout);

    return layout;
}

void Fat32VolumeValidator::validateFat32Format(const Fat32Bpb& bpb) const
{
    if (bpb.signatureFirstByte != fat32::kBootSignatureFirstByte ||
        bpb.signatureSecondByte != fat32::kBootSignatureSecondByte)
    {
        throw Fat32ValidationException(ValidationErrorCategory::Malformed,
            std::string("Invalid ") + fat32::kBootSignatureFieldName + ": expected 0x55AA");
    }

    if (bpb.rootEntryCount != 0)
    {
        throw Fat32ValidationException(ValidationErrorCategory::Malformed,
            std::string("Invalid FAT32 ") + fat32::kRootEntryCountFieldName + ": expected 0");
    }

    if (bpb.totalSectors16 != 0)
    {
        throw Fat32ValidationException(ValidationErrorCategory::Malformed,
            std::string("Invalid FAT32 ") + fat32::kTotalSectors16FieldName + ": expected 0");
    }

    if (bpb.fatSize16 != 0)
    {
        throw Fat32ValidationException(ValidationErrorCategory::Malformed,
            std::string("Invalid FAT32 ") + fat32::kFatSize16FieldName + ": expected 0");
    }

    if (bpb.fileSystemVersion != fat32::kFat32Version)
    {
        throw Fat32ValidationException(ValidationErrorCategory::Unsupported,
            std::string("Unsupported FAT32 ") + fat32::kFileSystemVersionFieldName + ": " +
                std::to_string(bpb.fileSystemVersion));
    }
}

void Fat32VolumeValidator::validateSectorAndClusterGeometry(
    const Fat32Bpb& bpb, Fat32VolumeLayout& layout) const
{
    switch (bpb.bytesPerSector)
    {
    case 512:
    case 1024:
    case 2048:
    case 4096:
        break;

    default:
        throw Fat32ValidationException(ValidationErrorCategory::Malformed,
            std::string("Invalid ") + fat32::kBytesPerSectorFieldName + ": " +
                std::to_string(bpb.bytesPerSector));
    }

    if (bpb.sectorsPerCluster == 0 || bpb.sectorsPerCluster > 128 ||
        (bpb.sectorsPerCluster & (bpb.sectorsPerCluster - 1)) != 0)
    {
        throw Fat32ValidationException(ValidationErrorCategory::Malformed,
            std::string("Invalid ") + fat32::kSectorsPerClusterFieldName + ": " +
                std::to_string(bpb.sectorsPerCluster));
    }

    layout.bytesPerSector = bpb.bytesPerSector;
    layout.sectorsPerCluster = bpb.sectorsPerCluster;

    layout.clusterSize = checkedMultiply(
        layout.bytesPerSector,
        layout.sectorsPerCluster,
        "cluster size");
}

void Fat32VolumeValidator::validateVolumeGeometry(
    const Fat32Bpb& bpb,
    std::uint64_t imageSize,
    Fat32VolumeLayout& layout) const
{
    if (bpb.totalSectors32 == 0)
    {
        throw Fat32ValidationException(ValidationErrorCategory::Malformed,
            std::string("Invalid ") + fat32::kTotalSectors32FieldName + ": 0");
    }

    layout.totalSectors = bpb.totalSectors32;
    layout.volumeSize = checkedMultiply(
        layout.totalSectors,
        layout.bytesPerSector,
        "volume size");

    if (layout.volumeSize > imageSize)
    {
        throw Fat32ValidationException(
            ValidationErrorCategory::Corrupted, "Declared volume extends beyond the image size");
    }
}

void Fat32VolumeValidator::validateFatGeometry(const Fat32Bpb& bpb, Fat32VolumeLayout& layout) const
{
    if (bpb.reservedSectorCount == 0)
    {
        throw Fat32ValidationException(ValidationErrorCategory::Malformed,
            std::string("Invalid ") + fat32::kReservedSectorCountFieldName + ": 0");
    }

    if (bpb.fatCount == 0)
    {
        throw Fat32ValidationException(ValidationErrorCategory::Malformed,
            std::string("Invalid ") + fat32::kFatCountFieldName + ": 0");
    }

    if (bpb.fatSize32 == 0)
    {
        throw Fat32ValidationException(ValidationErrorCategory::Malformed,
            std::string("Invalid ") + fat32::kFatSize32FieldName + ": 0");
    }

    layout.fatCount = bpb.fatCount;
    layout.fatSizeSectors = bpb.fatSize32;

    layout.fatSizeBytes = checkedMultiply(
        layout.fatSizeSectors,
        layout.bytesPerSector,
        "FAT size");

    layout.fatOffset = checkedMultiply(
        bpb.reservedSectorCount,
        layout.bytesPerSector,
        "FAT offset");

    const bool mirroringDisabled = (bpb.extendedFlags & fat32::kFatMirroringDisabledMask) != 0;

    if (mirroringDisabled)
    {
        layout.selectedFatIndex = bpb.extendedFlags & fat32::kActiveFatIndexMask;
    }
    else
    {
        layout.selectedFatIndex = 0;
    }

    if (layout.selectedFatIndex >= layout.fatCount)
    {
        throw Fat32ValidationException(
            ValidationErrorCategory::Corrupted, "Active FAT index is outside available FATs");
    }

    const std::uint64_t selectedFatRelativeOffset =
        checkedMultiply(
            layout.selectedFatIndex,
            layout.fatSizeBytes,
            "selected FAT relative offset");

    layout.selectedFatOffset =
        checkedAdd(
            layout.fatOffset,
            selectedFatRelativeOffset,
            "selected FAT offset");
}

void Fat32VolumeValidator::validateDataRegionGeometry(
    const Fat32Bpb& bpb, Fat32VolumeLayout& layout) const
{
    const std::uint64_t fatRegionSectors =
        checkedMultiply(
            layout.fatCount,
            layout.fatSizeSectors,
            "FAT region size");

    layout.firstDataSector =
        checkedAdd(
            bpb.reservedSectorCount,
            fatRegionSectors,
            "first data sector");

    if (layout.firstDataSector >= layout.totalSectors)
    {
        throw Fat32ValidationException(
            ValidationErrorCategory::Corrupted, "Data region starts outside the volume");
    }

    layout.dataSectors = layout.totalSectors - layout.firstDataSector;

    layout.dataOffset =
        checkedMultiply(
            layout.firstDataSector,
            layout.bytesPerSector,
            "data region offset");

    layout.clusterCount = layout.dataSectors / layout.sectorsPerCluster;

    if (layout.clusterCount < fat32::kMinFat32ClusterCount)
    {
        throw Fat32ValidationException(
            ValidationErrorCategory::Malformed, "Volume does not contain a FAT32 cluster count");
    }
}

void Fat32VolumeValidator::validateRootDirectoryGeometry(
    const Fat32Bpb& bpb, Fat32VolumeLayout& layout) const
{
    if (bpb.rootCluster < 2)
    {
        throw Fat32ValidationException(ValidationErrorCategory::Malformed,
            std::string("Invalid ") + fat32::kRootClusterFieldName + ": " +
                std::to_string(bpb.rootCluster));
    }

    const std::uint64_t rootClusterIndex = static_cast<std::uint64_t>(bpb.rootCluster - 2);

    if (rootClusterIndex >= layout.clusterCount)
    {
        throw Fat32ValidationException(
            ValidationErrorCategory::Corrupted, "Root cluster is outside the data cluster range");
    }

    layout.rootCluster = bpb.rootCluster;
}

std::uint64_t Fat32VolumeValidator::checkedMultiply(
    std::uint64_t lhs,
    std::uint64_t rhs,
    const char* operationName) const
{
    if (lhs != 0 && rhs > std::numeric_limits<std::uint64_t>::max() / lhs)
    {
        throw Fat32ValidationException(ValidationErrorCategory::Corrupted,
            std::string("FAT32 geometry multiplication overflow: ") + operationName);
    }

    return lhs * rhs;
}

std::uint64_t Fat32VolumeValidator::checkedAdd(
    std::uint64_t lhs,
    std::uint64_t rhs,
    const char* operationName) const
{
    if (rhs > std::numeric_limits<std::uint64_t>::max() - lhs)
    {
        throw Fat32ValidationException(ValidationErrorCategory::Corrupted,
            std::string("FAT32 geometry addition overflow: ") + operationName);
    }

    return lhs + rhs;
}

} // namespace fat32tree
