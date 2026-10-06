#include <fat32tree/BootSectorParser.h>
#include <fat32tree/Fat32Constants.h>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

namespace
{

class BootSectorParserTest : public ::testing::Test
{
protected:
    static constexpr std::size_t kBootSectorSize = 512;

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
                "Fat32Tree-BootSectorParser-" + std::to_string(timestamp) +
                "-" + std::to_string(nextId.fetch_add(1));
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
        m_imagePath = m_temporaryDirectory / "boot-sector.img";
    }

    void TearDown() override
    {
        std::error_code error;
        std::filesystem::remove_all(m_temporaryDirectory, error);
    }

    void writeImage(const std::vector<std::uint8_t>& bytes)
    {
        std::ofstream output(m_imagePath, std::ios::binary);
        ASSERT_TRUE(output.is_open());
        output.write(
            reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
        output.close();
        ASSERT_TRUE(output.good());
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

    std::vector<std::uint8_t> emptyBootSector() const
    {
        return std::vector<std::uint8_t>(kBootSectorSize, 0);
    }

    std::filesystem::path m_temporaryDirectory;
    std::filesystem::path m_imagePath;
};

TEST_F(BootSectorParserTest, ValidBootSectorParsesAllFields)
{
    auto bytes = emptyBootSector();
    writeU16LE(bytes, fat32tree::fat32::kBytesPerSectorOffset, 0x0200);
    writeU8(bytes, fat32tree::fat32::kSectorsPerClusterOffset, 0x03);
    writeU16LE(bytes, fat32tree::fat32::kReservedSectorCountOffset, 0x1234);
    writeU8(bytes, fat32tree::fat32::kFatCountOffset, 0x05);
    writeU16LE(bytes, fat32tree::fat32::kRootEntryCountOffset, 0x6789);
    writeU16LE(bytes, fat32tree::fat32::kTotalSectors16Offset, 0xABCD);
    writeU16LE(bytes, fat32tree::fat32::kFatSize16Offset, 0x1357);
    writeU32LE(bytes, fat32tree::fat32::kTotalSectors32Offset, 0x89ABCDEF);
    writeU32LE(bytes, fat32tree::fat32::kFatSize32Offset, 0x76543210);
    writeU16LE(bytes, fat32tree::fat32::kExtendedFlagsOffset, 0x2468);
    writeU16LE(bytes, fat32tree::fat32::kFileSystemVersionOffset, 0x369C);
    writeU32LE(bytes, fat32tree::fat32::kRootClusterOffset, 0x10293847);
    writeU16LE(bytes, fat32tree::fat32::kFsInfoSectorOffset, 0x55AA);
    writeU16LE(bytes, fat32tree::fat32::kBackupBootSectorOffset, 0xA55A);
    writeU8(bytes, fat32tree::fat32::kBootSignatureOffset, 0xC3);
    writeU8(bytes, fat32tree::fat32::kBootSignatureOffset + 1, 0x7E);
    writeImage(bytes);

    fat32tree::BinaryReader reader(m_imagePath);
    fat32tree::BootSectorParser parser(reader);
    const auto bpb = parser.parse();

    EXPECT_EQ(bpb.bytesPerSector, 0x0200);
    EXPECT_EQ(bpb.sectorsPerCluster, 0x03);
    EXPECT_EQ(bpb.reservedSectorCount, 0x1234);
    EXPECT_EQ(bpb.fatCount, 0x05);
    EXPECT_EQ(bpb.rootEntryCount, 0x6789);
    EXPECT_EQ(bpb.totalSectors16, 0xABCD);
    EXPECT_EQ(bpb.fatSize16, 0x1357);
    EXPECT_EQ(bpb.totalSectors32, 0x89ABCDEF);
    EXPECT_EQ(bpb.fatSize32, 0x76543210);
    EXPECT_EQ(bpb.extendedFlags, 0x2468);
    EXPECT_EQ(bpb.fileSystemVersion, 0x369C);
    EXPECT_EQ(bpb.rootCluster, 0x10293847);
    EXPECT_EQ(bpb.fsInfoSector, 0x55AA);
    EXPECT_EQ(bpb.backupBootSector, 0xA55A);
    EXPECT_EQ(bpb.signatureFirstByte, 0xC3);
    EXPECT_EQ(bpb.signatureSecondByte, 0x7E);
}

TEST_F(BootSectorParserTest, ParsesLittleEndianFieldsCorrectly)
{
    auto bytes = emptyBootSector();
    writeU16LE(bytes, fat32tree::fat32::kBytesPerSectorOffset, 0xBEEF);
    writeU32LE(bytes, fat32tree::fat32::kTotalSectors32Offset, 0x89ABCDEF);
    writeU32LE(bytes, fat32tree::fat32::kRootClusterOffset, 0x12345678);
    writeImage(bytes);

    fat32tree::BinaryReader reader(m_imagePath);
    fat32tree::BootSectorParser parser(reader);
    const auto bpb = parser.parse();

    EXPECT_EQ(bpb.bytesPerSector, 0xBEEF);
    EXPECT_EQ(bpb.totalSectors32, 0x89ABCDEF);
    EXPECT_EQ(bpb.rootCluster, 0x12345678);
}

TEST_F(BootSectorParserTest, TruncatedBootSectorThrows)
{
    std::vector<std::uint8_t> bytes(510, 0);
    writeImage(bytes);

    fat32tree::BinaryReader reader(m_imagePath);
    fat32tree::BootSectorParser parser(reader);

    EXPECT_THROW(parser.parse(), fat32tree::BootSectorParseException);
}

TEST_F(BootSectorParserTest, ExceptionContainsFieldName)
{
    std::vector<std::uint8_t> bytes(
        static_cast<std::size_t>(fat32tree::fat32::kRootClusterOffset + 2), 0);
    writeImage(bytes);

    fat32tree::BinaryReader reader(m_imagePath);
    fat32tree::BootSectorParser parser(reader);

    try
    {
        (void)parser.parse();
        FAIL() << "Expected BootSectorParseException";
    }
    catch (const fat32tree::BootSectorParseException& exception)
    {
        EXPECT_NE(
            std::string(exception.what()).find(
                fat32tree::fat32::kRootClusterFieldName),
            std::string::npos);
    }
}

TEST_F(BootSectorParserTest, ExceptionContainsReadStatus)
{
    std::vector<std::uint8_t> bytes(
        static_cast<std::size_t>(fat32tree::fat32::kRootClusterOffset + 2), 0);
    writeImage(bytes);

    fat32tree::BinaryReader reader(m_imagePath);
    fat32tree::BootSectorParser parser(reader);

    try
    {
        (void)parser.parse();
        FAIL() << "Expected BootSectorParseException";
    }
    catch (const fat32tree::BootSectorParseException& exception)
    {
        EXPECT_NE(std::string(exception.what()).find("OutOfBounds"), std::string::npos);
    }
}

TEST_F(BootSectorParserTest, InvalidSemanticValuesAreStillParsed)
{
    auto bytes = emptyBootSector();
    writeU16LE(bytes, fat32tree::fat32::kBytesPerSectorOffset, 0x1234);
    writeU8(bytes, fat32tree::fat32::kBootSignatureOffset, 0x00);
    writeU8(bytes, fat32tree::fat32::kBootSignatureOffset + 1, 0x00);
    writeImage(bytes);

    fat32tree::BinaryReader reader(m_imagePath);
    fat32tree::BootSectorParser parser(reader);
    const auto bpb = parser.parse();

    EXPECT_EQ(bpb.bytesPerSector, 0x1234);
    EXPECT_EQ(bpb.signatureFirstByte, 0x00);
    EXPECT_EQ(bpb.signatureSecondByte, 0x00);
}

} // namespace
