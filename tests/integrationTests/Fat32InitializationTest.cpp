#include <fat32tree/BinaryReader.h>
#include <fat32tree/BootSectorParser.h>
#include <fat32tree/Fat32Constants.h>
#include <fat32tree/Fat32VolumeValidator.h>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <vector>

namespace
{

class Fat32InitializationTest : public ::testing::Test
{
protected:
    static constexpr std::uint32_t kTotalSectors = 70000;
    static constexpr std::uint16_t kBytesPerSector = 512;
    static constexpr std::uint64_t kImageSize =
        static_cast<std::uint64_t>(kTotalSectors) * kBytesPerSector;

    void SetUp() override
    {
        static std::atomic<std::uint64_t> nextId{0};
        const auto timestamp = std::chrono::high_resolution_clock::now()
                                   .time_since_epoch()
                                   .count();
        const auto temporaryRoot = std::filesystem::temp_directory_path();

        for (int attempt = 0; attempt < 100; ++attempt)
        {
            const auto directoryName =
                "Fat32Tree-Initialization-" + std::to_string(timestamp) + "-" +
                std::to_string(nextId.fetch_add(1));
            const auto candidate = temporaryRoot / directoryName;
            std::error_code error;

            if (std::filesystem::create_directory(candidate, error))
            {
                m_temporaryDirectory = candidate;
                break;
            }

            ASSERT_FALSE(error) << error.message();
        }

        ASSERT_FALSE(m_temporaryDirectory.empty());
        m_imagePath = m_temporaryDirectory / "synthetic-fat32.img";

        std::ofstream image(m_imagePath, std::ios::binary);
        ASSERT_TRUE(image.is_open());

        std::vector<std::uint8_t> bootSector(512, 0);
        writeU16LE(
            bootSector,
            fat32tree::fat32::kBytesPerSectorOffset,
            kBytesPerSector);
        writeU8(
            bootSector,
            fat32tree::fat32::kSectorsPerClusterOffset,
            1);
        writeU16LE(
            bootSector,
            fat32tree::fat32::kReservedSectorCountOffset,
            32);
        writeU8(
            bootSector,
            fat32tree::fat32::kFatCountOffset,
            2);
        writeU16LE(
            bootSector,
            fat32tree::fat32::kRootEntryCountOffset,
            0);
        writeU16LE(
            bootSector,
            fat32tree::fat32::kTotalSectors16Offset,
            0);
        writeU16LE(
            bootSector,
            fat32tree::fat32::kFatSize16Offset,
            0);
        writeU32LE(
            bootSector,
            fat32tree::fat32::kTotalSectors32Offset,
            kTotalSectors);
        writeU32LE(
            bootSector,
            fat32tree::fat32::kFatSize32Offset,
            1000);
        writeU16LE(
            bootSector,
            fat32tree::fat32::kExtendedFlagsOffset,
            0);
        writeU16LE(
            bootSector,
            fat32tree::fat32::kFileSystemVersionOffset,
            0);
        writeU32LE(
            bootSector,
            fat32tree::fat32::kRootClusterOffset,
            2);
        writeU8(
            bootSector,
            fat32tree::fat32::kBootSignatureOffset,
            fat32tree::fat32::kBootSignatureFirstByte);
        writeU8(
            bootSector,
            fat32tree::fat32::kBootSignatureOffset + 1,
            fat32tree::fat32::kBootSignatureSecondByte);

        image.write(
            reinterpret_cast<const char*>(bootSector.data()),
            static_cast<std::streamsize>(bootSector.size()));
        ASSERT_TRUE(image.good());

        // Extend the file's logical length without writing the intervening
        // zero-filled volume data.
        image.seekp(static_cast<std::streamoff>(kImageSize - 1), std::ios::beg);
        ASSERT_TRUE(image.good());
        image.put('\0');
        ASSERT_TRUE(image.good());
        image.close();
        ASSERT_TRUE(image.good());
    }

    void TearDown() override
    {
        std::error_code error;
        std::filesystem::remove_all(m_temporaryDirectory, error);
    }

    static void writeU8(
        std::vector<std::uint8_t>& bytes,
        std::uint64_t offset,
        std::uint8_t value)
    {
        bytes.at(static_cast<std::size_t>(offset)) = value;
    }

    static void writeU16LE(
        std::vector<std::uint8_t>& bytes,
        std::uint64_t offset,
        std::uint16_t value)
    {
        const auto index = static_cast<std::size_t>(offset);
        bytes.at(index) = static_cast<std::uint8_t>(value & 0xFFU);
        bytes.at(index + 1) = static_cast<std::uint8_t>((value >> 8) & 0xFFU);
    }

    static void writeU32LE(
        std::vector<std::uint8_t>& bytes,
        std::uint64_t offset,
        std::uint32_t value)
    {
        const auto index = static_cast<std::size_t>(offset);
        bytes.at(index) = static_cast<std::uint8_t>(value & 0xFFU);
        bytes.at(index + 1) = static_cast<std::uint8_t>((value >> 8) & 0xFFU);
        bytes.at(index + 2) = static_cast<std::uint8_t>((value >> 16) & 0xFFU);
        bytes.at(index + 3) = static_cast<std::uint8_t>((value >> 24) & 0xFFU);
    }

    std::filesystem::path m_temporaryDirectory;
    std::filesystem::path m_imagePath;
};

TEST_F(Fat32InitializationTest, ReadsAndValidatesSyntheticFat32Image)
{
    fat32tree::BinaryReader reader(m_imagePath);
    ASSERT_EQ(reader.size(), kImageSize);

    fat32tree::BootSectorParser parser(reader);
    const fat32tree::Fat32Bpb bpb = parser.parse();

    EXPECT_EQ(bpb.bytesPerSector, 512U);
    EXPECT_EQ(bpb.sectorsPerCluster, 1U);
    EXPECT_EQ(bpb.reservedSectorCount, 32U);
    EXPECT_EQ(bpb.fatCount, 2U);
    EXPECT_EQ(bpb.rootEntryCount, 0U);
    EXPECT_EQ(bpb.totalSectors16, 0U);
    EXPECT_EQ(bpb.fatSize16, 0U);
    EXPECT_EQ(bpb.totalSectors32, 70000U);
    EXPECT_EQ(bpb.fatSize32, 1000U);
    EXPECT_EQ(bpb.extendedFlags, 0U);
    EXPECT_EQ(bpb.fileSystemVersion, 0U);
    EXPECT_EQ(bpb.rootCluster, 2U);
    EXPECT_EQ(bpb.signatureFirstByte, 0x55U);
    EXPECT_EQ(bpb.signatureSecondByte, 0xAAU);

    const fat32tree::Fat32VolumeValidator validator;
    const fat32tree::Fat32VolumeLayout layout =
        validator.validate(bpb, reader.size());

    EXPECT_EQ(layout.bytesPerSector, 512U);
    EXPECT_EQ(layout.sectorsPerCluster, 1U);
    EXPECT_EQ(layout.clusterSize, 512U);
    EXPECT_EQ(layout.totalSectors, 70000U);
    EXPECT_EQ(layout.volumeSize, 35840000U);
    EXPECT_EQ(layout.fatCount, 2U);
    EXPECT_EQ(layout.fatSizeSectors, 1000U);
    EXPECT_EQ(layout.fatSizeBytes, 512000U);
    EXPECT_EQ(layout.fatOffset, 16384U);
    EXPECT_EQ(layout.selectedFatIndex, 0U);
    EXPECT_EQ(layout.selectedFatOffset, 16384U);
    EXPECT_EQ(layout.firstDataSector, 2032U);
    EXPECT_EQ(layout.dataOffset, 1040384U);
    EXPECT_EQ(layout.dataSectors, 67968U);
    EXPECT_EQ(layout.clusterCount, 67968U);
    EXPECT_EQ(layout.rootCluster, 2U);
}

} // namespace
