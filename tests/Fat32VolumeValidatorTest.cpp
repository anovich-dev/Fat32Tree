#include "fat32tree/boot/Fat32VolumeValidator.h"
#include "fat32tree/core/Fat32Constants.h"

#include <array>
#include <cstdint>
#include <limits>
#include <string>

#include <gtest/gtest.h>

namespace
{

class Fat32VolumeValidatorTest : public ::testing::Test
{
protected:
    static fat32tree::Fat32Bpb makeValidBpb()
    {
        fat32tree::Fat32Bpb bpb;
        bpb.bytesPerSector = 512;
        bpb.sectorsPerCluster = 1;
        bpb.reservedSectorCount = 32;
        bpb.fatCount = 2;
        bpb.totalSectors16 = 0;
        bpb.fatSize16 = 0;
        bpb.totalSectors32 = 70000;
        bpb.fatSize32 = 1000;
        bpb.fileSystemVersion = fat32tree::fat32::kFat32Version;
        bpb.rootCluster = 2;
        bpb.signatureFirstByte = fat32tree::fat32::kBootSignatureFirstByte;
        bpb.signatureSecondByte = fat32tree::fat32::kBootSignatureSecondByte;
        return bpb;
    }

    static std::uint64_t validImageSize(const fat32tree::Fat32Bpb& bpb)
    {
        return static_cast<std::uint64_t>(bpb.totalSectors32) * bpb.bytesPerSector;
    }

    void expectFailure(const fat32tree::Fat32Bpb& bpb, std::uint64_t imageSize,
        fat32tree::ValidationErrorCategory expectedCategory,
        const std::string& expectedMessagePart = {}) const
    {
        fat32tree::Fat32VolumeValidator validator;

        try
        {
            (void)validator.validate(bpb, imageSize);
            FAIL() << "Expected Fat32ValidationException";
        }
        catch (const fat32tree::Fat32ValidationException& exception)
        {
            EXPECT_EQ(exception.category(), expectedCategory);
            if (!expectedMessagePart.empty())
            {
                EXPECT_NE(
                    std::string(exception.what()).find(expectedMessagePart), std::string::npos);
            }
        }
    }
};

TEST_F(Fat32VolumeValidatorTest, ValidBpbProducesExpectedVolumeLayout)
{
    const auto bpb = makeValidBpb();
    fat32tree::Fat32VolumeValidator validator;

    const auto layout = validator.validate(bpb, validImageSize(bpb));

    EXPECT_EQ(layout.bytesPerSector, 512U);
    EXPECT_EQ(layout.sectorsPerCluster, 1U);
    EXPECT_EQ(layout.clusterSize, 512U);
    EXPECT_EQ(layout.totalSectors, 70000U);
    EXPECT_EQ(layout.volumeSize, 70000U * 512U);
    EXPECT_EQ(layout.fatCount, 2U);
    EXPECT_EQ(layout.fatSizeSectors, 1000U);
    EXPECT_EQ(layout.fatSizeBytes, 1000U * 512U);
    EXPECT_EQ(layout.fatOffset, 32U * 512U);
    EXPECT_EQ(layout.selectedFatIndex, 0U);
    EXPECT_EQ(layout.selectedFatOffset, 32U * 512U);
    EXPECT_EQ(layout.firstDataSector, 2032U);
    EXPECT_EQ(layout.dataOffset, 2032U * 512U);
    EXPECT_EQ(layout.dataSectors, 67968U);
    EXPECT_EQ(layout.clusterCount, 67968U);
    EXPECT_EQ(layout.rootCluster, 2U);
}

TEST_F(Fat32VolumeValidatorTest, AcceptsEverySupportedBytesPerSectorValue)
{
    constexpr std::array<std::uint16_t, 4> supportedSizes{512, 1024, 2048, 4096};
    fat32tree::Fat32VolumeValidator validator;

    for (const auto bytesPerSector : supportedSizes)
    {
        auto bpb = makeValidBpb();
        bpb.bytesPerSector = bytesPerSector;

        const auto layout = validator.validate(bpb, validImageSize(bpb));

        EXPECT_EQ(layout.bytesPerSector, bytesPerSector);
        EXPECT_EQ(layout.volumeSize, validImageSize(bpb));
    }
}

TEST_F(Fat32VolumeValidatorTest, RejectsInvalidBytesPerSector)
{
    auto bpb = makeValidBpb();
    bpb.bytesPerSector = 513;

    expectFailure(bpb, validImageSize(bpb), fat32tree::ValidationErrorCategory::Malformed,
        fat32tree::fat32::kBytesPerSectorFieldName);
}

TEST_F(Fat32VolumeValidatorTest, AcceptsEveryLegalSectorsPerClusterValue)
{
    constexpr std::array<std::uint8_t, 8> legalValues{1, 2, 4, 8, 16, 32, 64, 128};
    fat32tree::Fat32VolumeValidator validator;

    for (const auto sectorsPerCluster : legalValues)
    {
        auto bpb = makeValidBpb();
        bpb.sectorsPerCluster = sectorsPerCluster;
        bpb.totalSectors32 = bpb.reservedSectorCount + bpb.fatCount * bpb.fatSize32 +
                             fat32tree::fat32::kMinFat32ClusterCount * sectorsPerCluster;

        const auto layout = validator.validate(bpb, validImageSize(bpb));

        EXPECT_EQ(layout.sectorsPerCluster, sectorsPerCluster);
        EXPECT_EQ(layout.clusterSize, 512U * sectorsPerCluster);
    }
}

TEST_F(Fat32VolumeValidatorTest, RejectsZeroNonPowerOfTwoAndOversizedCluster)
{
    constexpr std::array<std::uint8_t, 3> invalidValues{0, 3, 129};

    for (const auto sectorsPerCluster : invalidValues)
    {
        auto bpb = makeValidBpb();
        bpb.sectorsPerCluster = sectorsPerCluster;

        expectFailure(bpb, validImageSize(bpb), fat32tree::ValidationErrorCategory::Malformed,
            fat32tree::fat32::kSectorsPerClusterFieldName);
    }
}

TEST_F(Fat32VolumeValidatorTest, RejectsInvalidBootSignature)
{
    auto bpb = makeValidBpb();
    bpb.signatureSecondByte = 0;

    expectFailure(bpb, validImageSize(bpb), fat32tree::ValidationErrorCategory::Malformed,
        fat32tree::fat32::kBootSignatureFieldName);
}

TEST_F(Fat32VolumeValidatorTest, RejectsNonzeroFat32FixedRootEntryCount)
{
    auto bpb = makeValidBpb();
    bpb.rootEntryCount = 1;

    expectFailure(bpb, validImageSize(bpb), fat32tree::ValidationErrorCategory::Malformed,
        fat32tree::fat32::kRootEntryCountFieldName);
}

TEST_F(Fat32VolumeValidatorTest, RejectsNonzeroTotalSectors16)
{
    auto bpb = makeValidBpb();
    bpb.totalSectors16 = 1;

    expectFailure(bpb, validImageSize(bpb), fat32tree::ValidationErrorCategory::Malformed,
        fat32tree::fat32::kTotalSectors16FieldName);
}

TEST_F(Fat32VolumeValidatorTest, RejectsNonzeroFatSize16)
{
    auto bpb = makeValidBpb();
    bpb.fatSize16 = 1;

    expectFailure(bpb, validImageSize(bpb), fat32tree::ValidationErrorCategory::Malformed,
        fat32tree::fat32::kFatSize16FieldName);
}

TEST_F(Fat32VolumeValidatorTest, RejectsUnsupportedFileSystemVersion)
{
    auto bpb = makeValidBpb();
    bpb.fileSystemVersion = 1;

    expectFailure(bpb, validImageSize(bpb), fat32tree::ValidationErrorCategory::Unsupported,
        fat32tree::fat32::kFileSystemVersionFieldName);
}

TEST_F(Fat32VolumeValidatorTest, RejectsZeroTotalSectors32)
{
    auto bpb = makeValidBpb();
    bpb.totalSectors32 = 0;

    expectFailure(bpb, validImageSize(bpb), fat32tree::ValidationErrorCategory::Malformed,
        fat32tree::fat32::kTotalSectors32FieldName);
}

TEST_F(Fat32VolumeValidatorTest, RejectsZeroReservedSectorCount)
{
    auto bpb = makeValidBpb();
    bpb.reservedSectorCount = 0;

    expectFailure(bpb, validImageSize(bpb), fat32tree::ValidationErrorCategory::Malformed,
        fat32tree::fat32::kReservedSectorCountFieldName);
}

TEST_F(Fat32VolumeValidatorTest, RejectsZeroFatCount)
{
    auto bpb = makeValidBpb();
    bpb.fatCount = 0;

    expectFailure(bpb, validImageSize(bpb), fat32tree::ValidationErrorCategory::Malformed,
        fat32tree::fat32::kFatCountFieldName);
}

TEST_F(Fat32VolumeValidatorTest, AllowsSingleFat)
{
    auto bpb = makeValidBpb();
    bpb.fatCount = 1;
    fat32tree::Fat32VolumeValidator validator;

    const auto layout = validator.validate(bpb, validImageSize(bpb));

    EXPECT_EQ(layout.fatCount, 1U);
    EXPECT_EQ(layout.firstDataSector, bpb.reservedSectorCount + bpb.fatSize32);
}

TEST_F(Fat32VolumeValidatorTest, RejectsZeroFatSize32)
{
    auto bpb = makeValidBpb();
    bpb.fatSize32 = 0;

    expectFailure(bpb, validImageSize(bpb), fat32tree::ValidationErrorCategory::Malformed,
        fat32tree::fat32::kFatSize32FieldName);
}

TEST_F(Fat32VolumeValidatorTest, UsesFatZeroWhenMirroringIsEnabled)
{
    auto bpb = makeValidBpb();
    bpb.extendedFlags = 0x0001; // Active-FAT bits are ignored while mirroring.
    fat32tree::Fat32VolumeValidator validator;

    const auto layout = validator.validate(bpb, validImageSize(bpb));

    EXPECT_EQ(layout.selectedFatIndex, 0U);
    EXPECT_EQ(layout.selectedFatOffset, layout.fatOffset);
}

TEST_F(Fat32VolumeValidatorTest, SelectsConfiguredFatWhenMirroringIsDisabled)
{
    auto bpb = makeValidBpb();
    bpb.extendedFlags =
        static_cast<std::uint16_t>(fat32tree::fat32::kFatMirroringDisabledMask | 1U);
    fat32tree::Fat32VolumeValidator validator;

    const auto layout = validator.validate(bpb, validImageSize(bpb));

    EXPECT_EQ(layout.selectedFatIndex, 1U);
    EXPECT_EQ(layout.selectedFatOffset, layout.fatOffset + layout.fatSizeBytes);
}

TEST_F(Fat32VolumeValidatorTest, RejectsActiveFatIndexOutsideFatCount)
{
    auto bpb = makeValidBpb();
    bpb.extendedFlags =
        static_cast<std::uint16_t>(fat32tree::fat32::kFatMirroringDisabledMask | 2U);

    expectFailure(bpb, validImageSize(bpb), fat32tree::ValidationErrorCategory::Corrupted,
        "Active FAT index");
}

TEST_F(Fat32VolumeValidatorTest, RejectsFatRegionThatStartsOutsideVolume)
{
    auto bpb = makeValidBpb();
    bpb.fatSize32 = 35000;

    expectFailure(bpb, validImageSize(bpb), fat32tree::ValidationErrorCategory::Corrupted,
        "Data region starts outside");
}

TEST_F(Fat32VolumeValidatorTest, RejectsImageSmallerThanDeclaredVolume)
{
    const auto bpb = makeValidBpb();

    expectFailure(bpb, validImageSize(bpb) - 1, fat32tree::ValidationErrorCategory::Corrupted,
        "beyond the image size");
}

TEST_F(Fat32VolumeValidatorTest, AcceptsImageExactlyAsLargeAsDeclaredVolume)
{
    const auto bpb = makeValidBpb();
    fat32tree::Fat32VolumeValidator validator;

    const auto layout = validator.validate(bpb, validImageSize(bpb));

    EXPECT_EQ(layout.volumeSize, validImageSize(bpb));
}

TEST_F(Fat32VolumeValidatorTest, RejectsVolumeWithFewerThanFat32MinimumClusters)
{
    auto bpb = makeValidBpb();
    bpb.totalSectors32 = bpb.reservedSectorCount + bpb.fatCount * bpb.fatSize32 +
                         fat32tree::fat32::kMinFat32ClusterCount - 1;

    expectFailure(bpb, validImageSize(bpb), fat32tree::ValidationErrorCategory::Malformed,
        "FAT32 cluster count");
}

TEST_F(Fat32VolumeValidatorTest, AcceptsMinimumFat32ClusterCountAndHighestValidRootCluster)
{
    auto bpb = makeValidBpb();
    bpb.totalSectors32 = bpb.reservedSectorCount + bpb.fatCount * bpb.fatSize32 +
                         fat32tree::fat32::kMinFat32ClusterCount;
    bpb.rootCluster = fat32tree::fat32::kMinFat32ClusterCount + 1;
    fat32tree::Fat32VolumeValidator validator;

    const auto layout = validator.validate(bpb, validImageSize(bpb));

    EXPECT_EQ(layout.clusterCount, fat32tree::fat32::kMinFat32ClusterCount);
    EXPECT_EQ(layout.rootCluster, bpb.rootCluster);
}

TEST_F(Fat32VolumeValidatorTest, RejectsRootClusterBelowTwo)
{
    auto bpb = makeValidBpb();
    bpb.rootCluster = 1;

    expectFailure(bpb, validImageSize(bpb), fat32tree::ValidationErrorCategory::Malformed,
        fat32tree::fat32::kRootClusterFieldName);
}

TEST_F(Fat32VolumeValidatorTest, RejectsRootClusterPastDataClusterRange)
{
    auto bpb = makeValidBpb();
    const auto clusterCount =
        bpb.totalSectors32 - bpb.reservedSectorCount - bpb.fatCount * bpb.fatSize32;
    bpb.rootCluster = clusterCount + 2;

    expectFailure(bpb, validImageSize(bpb), fat32tree::ValidationErrorCategory::Corrupted,
        "Root cluster is outside");
}

TEST_F(Fat32VolumeValidatorTest, RejectsMaximumDeclaredVolumeLargerThanImage)
{
    auto bpb = makeValidBpb();
    bpb.totalSectors32 = std::numeric_limits<std::uint32_t>::max();
    bpb.bytesPerSector = 4096;

    // BPB field widths bound this product well below uint64_t's limit; reject
    // the mismatch with the actual image size instead.
    expectFailure(bpb, validImageSize(makeValidBpb()),
        fat32tree::ValidationErrorCategory::Corrupted, "beyond the image size");
}

TEST_F(Fat32VolumeValidatorTest, AcceptsImageLargerThanDeclaredVolume)
{
    auto bpb = makeValidBpb();
    fat32tree::Fat32VolumeValidator validator;

    const auto layout = validator.validate(bpb, validImageSize(bpb) + 4096);

    EXPECT_EQ(layout.volumeSize, validImageSize(bpb));
}

} // namespace
