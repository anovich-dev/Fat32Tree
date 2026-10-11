#include "fat32tree/boot/BootSectorParser.h"

#include "fat32tree/core/BinaryReader.h"
#include "fat32tree/core/Fat32Constants.h"

namespace fat32tree
{

BootSectorParser::BootSectorParser(BinaryReader& reader) : m_reader(reader)
{
}

Fat32Bpb BootSectorParser::parse()
{
    Fat32Bpb bpb;

    bpb.bytesPerSector = requireRead(
        m_reader.readU16LE(fat32::kBytesPerSectorOffset), fat32::kBytesPerSectorFieldName);

    bpb.sectorsPerCluster = requireRead(
        m_reader.readU8(fat32::kSectorsPerClusterOffset), fat32::kSectorsPerClusterFieldName);

    bpb.reservedSectorCount = requireRead(m_reader.readU16LE(fat32::kReservedSectorCountOffset),
        fat32::kReservedSectorCountFieldName);

    bpb.fatCount = requireRead(m_reader.readU8(fat32::kFatCountOffset), fat32::kFatCountFieldName);

    bpb.rootEntryCount = requireRead(
        m_reader.readU16LE(fat32::kRootEntryCountOffset), fat32::kRootEntryCountFieldName);

    bpb.totalSectors16 = requireRead(
        m_reader.readU16LE(fat32::kTotalSectors16Offset), fat32::kTotalSectors16FieldName);

    bpb.fatSize16 =
        requireRead(m_reader.readU16LE(fat32::kFatSize16Offset), fat32::kFatSize16FieldName);

    bpb.totalSectors32 = requireRead(
        m_reader.readU32LE(fat32::kTotalSectors32Offset), fat32::kTotalSectors32FieldName);

    bpb.fatSize32 =
        requireRead(m_reader.readU32LE(fat32::kFatSize32Offset), fat32::kFatSize32FieldName);

    bpb.extendedFlags = requireRead(
        m_reader.readU16LE(fat32::kExtendedFlagsOffset), fat32::kExtendedFlagsFieldName);

    bpb.fileSystemVersion = requireRead(
        m_reader.readU16LE(fat32::kFileSystemVersionOffset), fat32::kFileSystemVersionFieldName);

    bpb.rootCluster =
        requireRead(m_reader.readU32LE(fat32::kRootClusterOffset), fat32::kRootClusterFieldName);

    bpb.fsInfoSector =
        requireRead(m_reader.readU16LE(fat32::kFsInfoSectorOffset), fat32::kFsInfoSectorFieldName);

    bpb.backupBootSector = requireRead(
        m_reader.readU16LE(fat32::kBackupBootSectorOffset), fat32::kBackupBootSectorFieldName);

    bpb.signatureFirstByte =
        requireRead(m_reader.readU8(fat32::kBootSignatureOffset), fat32::kBootSignatureFieldName);

    bpb.signatureSecondByte = requireRead(
        m_reader.readU8(fat32::kBootSignatureOffset + 1), fat32::kBootSignatureFieldName);

    return bpb;
}

} // namespace fat32tree
