#include <fat32tree/BinaryReader.h>

#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <system_error>

namespace
{

class BinaryReaderTest : public ::testing::Test
{
protected:
    static constexpr std::array<std::uint8_t, 10> kBytes{
        0x01, 0x02, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12, 0xAB, 0xFF};

    void SetUp() override
    {
        static std::atomic<std::uint64_t> nextId{0};
        const auto timestamp = std::chrono::high_resolution_clock::now()
                                   .time_since_epoch()
                                   .count();
        const auto temporaryRoot = std::filesystem::temp_directory_path();

        for (int attempt = 0; attempt < 100; ++attempt)
        {
            const std::string directoryName =
                "Fat32Tree-BinaryReader-" + std::to_string(timestamp) + "-" +
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
        m_filePath = m_temporaryDirectory / "binary-data.img";

        std::ofstream output(m_filePath, std::ios::binary);
        ASSERT_TRUE(output.is_open());
        output.write(
            reinterpret_cast<const char*>(kBytes.data()),
            static_cast<std::streamsize>(kBytes.size()));
        output.close();
        ASSERT_TRUE(output.good());
    }

    void TearDown() override
    {
        std::error_code error;
        std::filesystem::remove_all(m_temporaryDirectory, error);
    }

    std::filesystem::path m_temporaryDirectory;
    std::filesystem::path m_filePath;
};

TEST_F(BinaryReaderTest, OpensExistingBinaryFile)
{
    EXPECT_NO_THROW(fat32tree::BinaryReader reader(m_filePath));
}

TEST_F(BinaryReaderTest, ReportsExactFileSize)
{
    const fat32tree::BinaryReader reader(m_filePath);

    EXPECT_EQ(reader.size(), kBytes.size());
}

TEST_F(BinaryReaderTest, OpensEmptyFileAndReportsZeroSize)
{
    const auto emptyPath = m_temporaryDirectory / "empty.img";
    std::ofstream output(emptyPath, std::ios::binary);
    ASSERT_TRUE(output.is_open());
    output.close();
    ASSERT_TRUE(output.good());

    const fat32tree::BinaryReader reader(emptyPath);

    EXPECT_EQ(reader.size(), 0U);
}

TEST_F(BinaryReaderTest, ReadsFirstByte)
{
    fat32tree::BinaryReader reader(m_filePath);

    const auto result = reader.readU8(0);

    EXPECT_EQ(result.status, fat32tree::ReadStatus::Success);
    EXPECT_EQ(result.value, 0x01);
}

TEST_F(BinaryReaderTest, ReadsByteFromMiddle)
{
    fat32tree::BinaryReader reader(m_filePath);

    const auto result = reader.readU8(4);

    EXPECT_EQ(result.status, fat32tree::ReadStatus::Success);
    EXPECT_EQ(result.value, 0x78);
}

TEST_F(BinaryReaderTest, ReadsFinalByte)
{
    fat32tree::BinaryReader reader(m_filePath);

    const auto result = reader.readU8(kBytes.size() - 1);

    EXPECT_EQ(result.status, fat32tree::ReadStatus::Success);
    EXPECT_EQ(result.value, 0xFF);
}

TEST_F(BinaryReaderTest, ReadsByteAboveSignedCharRangeAsUnsigned)
{
    fat32tree::BinaryReader reader(m_filePath);

    const auto result = reader.readU8(8);

    EXPECT_EQ(result.status, fat32tree::ReadStatus::Success);
    EXPECT_EQ(result.value, 0xAB);
}

TEST_F(BinaryReaderTest, ReadsU16AsLittleEndian)
{
    fat32tree::BinaryReader reader(m_filePath);

    const auto result = reader.read16LE(2);

    EXPECT_EQ(result.status, fat32tree::ReadStatus::Success);
    EXPECT_EQ(result.value, 0x1234);
}

TEST_F(BinaryReaderTest, ReadsU32AsLittleEndian)
{
    fat32tree::BinaryReader reader(m_filePath);

    const auto result = reader.read32LE(4);

    EXPECT_EQ(result.status, fat32tree::ReadStatus::Success);
    EXPECT_EQ(result.value, 0x12345678);
}

TEST_F(BinaryReaderTest, ReadU8AtLastValidOffsetSucceeds)
{
    fat32tree::BinaryReader reader(m_filePath);

    const auto result = reader.readU8(kBytes.size() - 1);

    EXPECT_EQ(result.status, fat32tree::ReadStatus::Success);
    EXPECT_EQ(result.value, 0xFF);
}

TEST_F(BinaryReaderTest, ReadU8AtFileSizeReturnsOutOfBounds)
{
    fat32tree::BinaryReader reader(m_filePath);

    EXPECT_EQ(reader.readU8(kBytes.size()).status, fat32tree::ReadStatus::OutOfBounds);
}

TEST_F(BinaryReaderTest, ReadU8PastFileSizeReturnsOutOfBounds)
{
    fat32tree::BinaryReader reader(m_filePath);

    EXPECT_EQ(reader.readU8(kBytes.size() + 1).status, fat32tree::ReadStatus::OutOfBounds);
}

TEST_F(BinaryReaderTest, ReadU8AtMaximumOffsetReturnsOutOfBounds)
{
    fat32tree::BinaryReader reader(m_filePath);

    EXPECT_EQ(
        reader.readU8(std::numeric_limits<std::uint64_t>::max()).status,
        fat32tree::ReadStatus::OutOfBounds);
}

TEST_F(BinaryReaderTest, ReadU16AtLastValidOffsetSucceeds)
{
    fat32tree::BinaryReader reader(m_filePath);

    const auto result = reader.read16LE(kBytes.size() - 2);

    EXPECT_EQ(result.status, fat32tree::ReadStatus::Success);
    EXPECT_EQ(result.value, 0xFFAB);
}

TEST_F(BinaryReaderTest, ReadU16StartingOneByteBeforeEndReturnsOutOfBounds)
{
    fat32tree::BinaryReader reader(m_filePath);

    EXPECT_EQ(reader.read16LE(kBytes.size() - 1).status, fat32tree::ReadStatus::OutOfBounds);
}

TEST_F(BinaryReaderTest, ReadU16StartingAtFileSizeReturnsOutOfBounds)
{
    fat32tree::BinaryReader reader(m_filePath);

    EXPECT_EQ(reader.read16LE(kBytes.size()).status, fat32tree::ReadStatus::OutOfBounds);
}

TEST_F(BinaryReaderTest, ReadU32AtLastValidOffsetSucceeds)
{
    fat32tree::BinaryReader reader(m_filePath);

    const auto result = reader.read32LE(kBytes.size() - 4);

    EXPECT_EQ(result.status, fat32tree::ReadStatus::Success);
    EXPECT_EQ(result.value, 0xFFAB1234);
}

TEST_F(BinaryReaderTest, ReadU32StartingThreeBytesBeforeEndReturnsOutOfBounds)
{
    fat32tree::BinaryReader reader(m_filePath);

    EXPECT_EQ(reader.read32LE(kBytes.size() - 3).status, fat32tree::ReadStatus::OutOfBounds);
}

TEST_F(BinaryReaderTest, ReadU32StartingAtFileSizeReturnsOutOfBounds)
{
    fat32tree::BinaryReader reader(m_filePath);

    EXPECT_EQ(reader.read32LE(kBytes.size()).status, fat32tree::ReadStatus::OutOfBounds);
}

TEST_F(BinaryReaderTest, ReadsFromEmptyFileReturnOutOfBounds)
{
    const auto emptyPath = m_temporaryDirectory / "empty-reads.img";
    std::ofstream output(emptyPath, std::ios::binary);
    ASSERT_TRUE(output.is_open());
    output.close();
    ASSERT_TRUE(output.good());

    fat32tree::BinaryReader reader(emptyPath);

    EXPECT_EQ(reader.size(), 0U);
    EXPECT_EQ(reader.readU8(0).status, fat32tree::ReadStatus::OutOfBounds);
    EXPECT_EQ(reader.read16LE(0).status, fat32tree::ReadStatus::OutOfBounds);
    EXPECT_EQ(reader.read32LE(0).status, fat32tree::ReadStatus::OutOfBounds);
}

TEST_F(BinaryReaderTest, MissingFileThrowsBinaryReaderException)
{
    const auto missingPath = m_temporaryDirectory / "does-not-exist.img";

    EXPECT_THROW(
        fat32tree::BinaryReader reader(missingPath),
        fat32tree::BinaryReaderException);
}

TEST_F(BinaryReaderTest, ReadsUseIndependentAbsoluteOffsets)
{
    fat32tree::BinaryReader reader(m_filePath);

    const auto first = reader.readU8(0);
    const auto word = reader.read32LE(4);
    const auto halfWord = reader.read16LE(2);
    const auto final = reader.readU8(7);

    EXPECT_EQ(first.status, fat32tree::ReadStatus::Success);
    EXPECT_EQ(first.value, 0x01);
    EXPECT_EQ(word.status, fat32tree::ReadStatus::Success);
    EXPECT_EQ(word.value, 0x12345678);
    EXPECT_EQ(halfWord.status, fat32tree::ReadStatus::Success);
    EXPECT_EQ(halfWord.value, 0x1234);
    EXPECT_EQ(final.status, fat32tree::ReadStatus::Success);
    EXPECT_EQ(final.value, 0x12);
}

TEST_F(BinaryReaderTest, ValidReadSucceedsAfterOutOfBoundsRead)
{
    fat32tree::BinaryReader reader(m_filePath);

    const auto first = reader.readU8(0);
    const auto invalid = reader.read32LE(kBytes.size() - 3);
    const auto second = reader.read16LE(2);

    EXPECT_EQ(first.status, fat32tree::ReadStatus::Success);
    EXPECT_EQ(first.value, 0x01);
    EXPECT_EQ(invalid.status, fat32tree::ReadStatus::OutOfBounds);
    EXPECT_EQ(second.status, fat32tree::ReadStatus::Success);
    EXPECT_EQ(second.value, 0x1234);
}

} // namespace
