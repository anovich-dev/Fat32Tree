#pragma once

#include <cstdint>

namespace fat32tree::fat32
{

    // BPB field offsets
    inline constexpr std::uint64_t kBytesPerSectorOffset = 11;
    inline constexpr std::uint64_t kSectorsPerClusterOffset = 13;
    inline constexpr std::uint64_t kReservedSectorCountOffset = 14;
    inline constexpr std::uint64_t kFatCountOffset = 16;
    inline constexpr std::uint64_t kRootEntryCountOffset = 17;
    inline constexpr std::uint64_t kTotalSectors16Offset = 19;
    inline constexpr std::uint64_t kFatSize16Offset = 22;
    inline constexpr std::uint64_t kTotalSectors32Offset = 32;
    inline constexpr std::uint64_t kFatSize32Offset = 36;
    inline constexpr std::uint64_t kExtendedFlagsOffset = 40;
    inline constexpr std::uint64_t kFileSystemVersionOffset = 42;
    inline constexpr std::uint64_t kRootClusterOffset = 44;
    inline constexpr std::uint64_t kFsInfoSectorOffset = 48;
    inline constexpr std::uint64_t kBackupBootSectorOffset = 50;
    inline constexpr std::uint64_t kBootSignatureOffset = 510;

    // FAT specification field names
    inline constexpr const char *kBytesPerSectorFieldName =
        "BPB_BytsPerSec";

    inline constexpr const char *kSectorsPerClusterFieldName =
        "BPB_SecPerClus";

    inline constexpr const char *kReservedSectorCountFieldName =
        "BPB_RsvdSecCnt";

    inline constexpr const char *kFatCountFieldName =
        "BPB_NumFATs";

    inline constexpr const char *kRootEntryCountFieldName =
        "BPB_RootEntCnt";

    inline constexpr const char *kTotalSectors16FieldName =
        "BPB_TotSec16";

    inline constexpr const char *kFatSize16FieldName =
        "BPB_FATSz16";

    inline constexpr const char *kTotalSectors32FieldName =
        "BPB_TotSec32";

    inline constexpr const char *kFatSize32FieldName =
        "BPB_FATSz32";

    inline constexpr const char *kExtendedFlagsFieldName =
        "BPB_ExtFlags";

    inline constexpr const char *kFileSystemVersionFieldName =
        "BPB_FSVer";

    inline constexpr const char *kRootClusterFieldName =
        "BPB_RootClus";

    inline constexpr const char *kFsInfoSectorFieldName =
        "BPB_FSInfo";

    inline constexpr const char *kBackupBootSectorFieldName =
        "BPB_BkBootSec";

    inline constexpr const char *kBootSignatureFieldName =
        "Signature_word";

    // FAT32 specification and validation constants
    inline constexpr std::uint16_t kFat32Version = 0;
    inline constexpr std::uint32_t kMinFat32ClusterCount = 65525;

    // Expected boot signature values
    inline constexpr std::uint8_t kBootSignatureFirstByte = 0x55;
    inline constexpr std::uint8_t kBootSignatureSecondByte = 0xAA;

    // FAT32 extended flags constants
    inline constexpr std::uint16_t kFatMirroringDisabledMask = 0x0080;
    inline constexpr std::uint16_t kActiveFatIndexMask = 0x000F;

    // FAT32 entry constants
    inline constexpr std::uint32_t kFatEntrySize = 4;
    inline constexpr std::uint32_t kFatEntryValueMask = 0x0FFFFFFF;

    inline constexpr std::uint32_t kFreeClusterValue = 0x00000000;
    inline constexpr std::uint32_t kReservedClusterValue = 0x00000001;

    inline constexpr std::uint32_t kBadClusterValue = 0x0FFFFFF7;

    inline constexpr std::uint32_t kEndOfChainMin = 0x0FFFFFF8;
    inline constexpr std::uint32_t kEndOfChainMax = 0x0FFFFFFF;

    inline constexpr std::uint32_t kReservedClusterMin = 0x0FFFFFF0;
    inline constexpr std::uint32_t kReservedClusterMax = 0x0FFFFFF6;

} // namespace fat32tree::fat32