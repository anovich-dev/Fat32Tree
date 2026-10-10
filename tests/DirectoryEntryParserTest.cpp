#include <fat32tree/BinaryReader.h>
#include <fat32tree/DirectoryEntryParser.h>
#include <fat32tree/Fat32Constants.h>
#include <fat32tree/IDirectoryEntryValidator.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "Mocks/DirectoryEntryValidatorMock.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace
{

using ::testing::_;
using ::testing::Return;

class DirectoryEntryParserTest : public ::testing::Test
{
protected:
    static constexpr std::size_t kEntrySize =
        static_cast<std::size_t>(fat32tree::fat32::kDirectoryEntrySize);

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
                "Fat32Tree-DirectoryEntryParser-" + std::to_string(timestamp) +
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
        m_imagePath = m_temporaryDirectory / "directory-entry.bin";
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

    static std::vector<std::uint8_t> makeEntry(
        const std::array<std::uint8_t, fat32tree::fat32::kShortNameLength>& name,
        std::uint8_t attributes = 0)
    {
        std::vector<std::uint8_t> bytes(kEntrySize, 0);
        std::copy(name.begin(), name.end(), bytes.begin());
        bytes[fat32tree::fat32::kDirectoryAttributeOffset] = attributes;
        return bytes;
    }

    static std::array<std::uint8_t, fat32tree::fat32::kShortNameLength>
    paddedName(const char* name)
    {
        std::array<std::uint8_t, fat32tree::fat32::kShortNameLength> result{};
        result.fill(static_cast<std::uint8_t>(' '));
        const std::string value(name);
        std::copy_n(value.begin(), std::min(value.size(), result.size()), result.begin());
        return result;
    }

    static std::array<std::uint8_t, fat32tree::fat32::kShortNameLength>
    shortName(const char* base, const char* extension)
    {
        std::array<std::uint8_t, fat32tree::fat32::kShortNameLength> result{};
        result.fill(static_cast<std::uint8_t>(' '));

        const std::string baseString(base);
        const std::string extensionString(extension);
        std::copy_n(
            baseString.begin(),
            std::min(baseString.size(), fat32tree::fat32::kShortNameBaseLength),
            result.begin());
        std::copy_n(
            extensionString.begin(),
            std::min(extensionString.size(), fat32tree::fat32::kShortNameExtensionLength),
            result.begin() + fat32tree::fat32::kShortNameBaseLength);
        return result;
    }

    static void writeU16LE(
        std::vector<std::uint8_t>& bytes,
        std::size_t offset,
        std::uint16_t value)
    {
        ASSERT_LT(offset + 1, bytes.size());
        bytes[offset] = static_cast<std::uint8_t>(value & 0xFFU);
        bytes[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFFU);
    }

    static void writeU32LE(
        std::vector<std::uint8_t>& bytes,
        std::size_t offset,
        std::uint32_t value)
    {
        ASSERT_LE(offset + 4, bytes.size());
        bytes[offset] = static_cast<std::uint8_t>(value & 0xFFU);
        bytes[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFFU);
        bytes[offset + 2] = static_cast<std::uint8_t>((value >> 16) & 0xFFU);
        bytes[offset + 3] = static_cast<std::uint8_t>((value >> 24) & 0xFFU);
    }

    std::filesystem::path m_temporaryDirectory;
    std::filesystem::path m_imagePath;
};

TEST_F(DirectoryEntryParserTest, ReturnsInvalidWhenNameBytesCannotBeRead)
{
    writeImage(std::vector<std::uint8_t>(10, 0));
    fat32tree::BinaryReader reader(m_imagePath);
    auto validator = std::make_unique<fat32tree::test::DirectoryEntryValidatorMock>();
    EXPECT_CALL(*validator, isValidShortName(_)).Times(0);
    fat32tree::DirectoryEntryParser parser(reader, std::move(validator));

    const auto result = parser.parse(0);

    EXPECT_EQ(result.state, fat32tree::DirectoryEntryState::Invalid);
}

TEST_F(DirectoryEntryParserTest, EndMarkerReturnsEndWithoutCallingValidator)
{
    auto bytes = makeEntry(paddedName("END"));
    bytes[0] = fat32tree::fat32::kDirectoryEntryEndMarker;
    writeImage(bytes);
    fat32tree::BinaryReader reader(m_imagePath);
    auto validator = std::make_unique<fat32tree::test::DirectoryEntryValidatorMock>();
    EXPECT_CALL(*validator, isValidShortName(_)).Times(0);
    fat32tree::DirectoryEntryParser parser(reader, std::move(validator));

    const auto result = parser.parse(0);

    EXPECT_EQ(result.state, fat32tree::DirectoryEntryState::End);
}

TEST_F(DirectoryEntryParserTest, DeletedMarkerReturnsDeletedWithoutCallingValidator)
{
    auto bytes = makeEntry(paddedName("REMOVED"));
    bytes[0] = fat32tree::fat32::kDirectoryEntryDeletedMarker;
    writeImage(bytes);
    fat32tree::BinaryReader reader(m_imagePath);
    auto validator = std::make_unique<fat32tree::test::DirectoryEntryValidatorMock>();
    EXPECT_CALL(*validator, isValidShortName(_)).Times(0);
    fat32tree::DirectoryEntryParser parser(reader, std::move(validator));

    const auto result = parser.parse(0);

    EXPECT_EQ(result.state, fat32tree::DirectoryEntryState::Deleted);
}

TEST_F(DirectoryEntryParserTest, KanjiMarkerIsNotClassifiedAsDeleted)
{
    auto bytes = makeEntry(paddedName("README"));
    bytes[0] = fat32tree::fat32::kDirectoryEntryKanjiMarker;
    writeImage(bytes);
    fat32tree::BinaryReader reader(m_imagePath);
    auto validator = std::make_unique<fat32tree::test::DirectoryEntryValidatorMock>();
    EXPECT_CALL(*validator, isValidShortName(_)).WillOnce(Return(true));
    fat32tree::DirectoryEntryParser parser(reader, std::move(validator));

    const auto result = parser.parse(0);

    EXPECT_NE(result.state, fat32tree::DirectoryEntryState::Deleted);
    EXPECT_EQ(result.state, fat32tree::DirectoryEntryState::Normal);
}

TEST_F(DirectoryEntryParserTest, ReturnsInvalidWhenAttributesCannotBeRead)
{
    auto bytes = makeEntry(paddedName("FILE"));
    bytes.resize(fat32tree::fat32::kDirectoryAttributeOffset);
    writeImage(bytes);
    fat32tree::BinaryReader reader(m_imagePath);
    auto validator = std::make_unique<fat32tree::test::DirectoryEntryValidatorMock>();
    EXPECT_CALL(*validator, isValidShortName(_)).Times(0);
    fat32tree::DirectoryEntryParser parser(reader, std::move(validator));

    const auto result = parser.parse(0);

    EXPECT_EQ(result.state, fat32tree::DirectoryEntryState::Invalid);
}

TEST_F(DirectoryEntryParserTest, LongNameEntryPrecedesVolumeLabelAndSkipsValidator)
{
    auto bytes = makeEntry(
        paddedName("LONGNAME"),
        fat32tree::fat32::kAttributeLongName);
    writeImage(bytes);
    fat32tree::BinaryReader reader(m_imagePath);
    auto validator = std::make_unique<fat32tree::test::DirectoryEntryValidatorMock>();
    EXPECT_CALL(*validator, isValidShortName(_)).Times(0);
    fat32tree::DirectoryEntryParser parser(reader, std::move(validator));

    const auto result = parser.parse(0);

    EXPECT_EQ(result.state, fat32tree::DirectoryEntryState::LongFileName);
}

TEST_F(DirectoryEntryParserTest, VolumeLabelUsesDedicatedParsingAndSkipsValidator)
{
    auto bytes = makeEntry(
        paddedName("MY_USB"),
        fat32tree::fat32::kAttributeVolumeId);
    writeImage(bytes);
    fat32tree::BinaryReader reader(m_imagePath);
    auto validator = std::make_unique<fat32tree::test::DirectoryEntryValidatorMock>();
    EXPECT_CALL(*validator, isValidShortName(_)).Times(0);
    fat32tree::DirectoryEntryParser parser(reader, std::move(validator));

    const auto result = parser.parse(0);

    ASSERT_EQ(result.state, fat32tree::DirectoryEntryState::VolumeLabel);
    EXPECT_EQ(result.entry.name, "MY_USB");
    EXPECT_EQ(result.entry.type, fat32tree::DirectoryEntryType::Directory);
    EXPECT_EQ(result.entry.firstCluster, 0U);
    EXPECT_EQ(result.entry.fileSize, 0U);
}

TEST_F(DirectoryEntryParserTest, ParsesVolumeLabelWithoutShortNameDotFormatting)
{
    auto bytes = makeEntry(
        paddedName("MYVOLUMEABC"),
        fat32tree::fat32::kAttributeVolumeId);
    writeImage(bytes);
    fat32tree::BinaryReader reader(m_imagePath);
    auto validator = std::make_unique<fat32tree::test::DirectoryEntryValidatorMock>();
    EXPECT_CALL(*validator, isValidShortName(_)).Times(0);
    fat32tree::DirectoryEntryParser parser(reader, std::move(validator));

    const auto result = parser.parse(0);

    ASSERT_EQ(result.state, fat32tree::DirectoryEntryState::VolumeLabel);
    EXPECT_EQ(result.entry.name, "MYVOLUMEABC");
    EXPECT_NE(result.entry.name, "MYVOLUM.EABC");
}

TEST_F(DirectoryEntryParserTest, ValidatorRejectingReadableShortNameReturnsInvalid)
{
    auto bytes = makeEntry(shortName("README", "TXT"));
    writeImage(bytes);
    fat32tree::BinaryReader reader(m_imagePath);
    auto validator = std::make_unique<fat32tree::test::DirectoryEntryValidatorMock>();
    EXPECT_CALL(*validator, isValidShortName(_)).WillOnce(Return(false));
    fat32tree::DirectoryEntryParser parser(reader, std::move(validator));

    const auto result = parser.parse(0);

    EXPECT_EQ(result.state, fat32tree::DirectoryEntryState::Invalid);
}

TEST_F(DirectoryEntryParserTest, ParsesValidShortNameAsFileAndCombinesClusterAndSize)
{
    auto bytes = makeEntry(shortName("README", "TXT"), fat32tree::fat32::kAttributeArchive);
    writeU16LE(
        bytes,
        fat32tree::fat32::kDirectoryFirstClusterHighOffset,
        0x1234);
    writeU16LE(
        bytes,
        fat32tree::fat32::kDirectoryFirstClusterLowOffset,
        0x5678);
    writeU32LE(bytes, fat32tree::fat32::kDirectoryFileSizeOffset, 0x89ABCDEF);
    writeImage(bytes);
    fat32tree::BinaryReader reader(m_imagePath);
    auto validator = std::make_unique<fat32tree::test::DirectoryEntryValidatorMock>();
    EXPECT_CALL(*validator, isValidShortName(_)).WillOnce(Return(true));
    fat32tree::DirectoryEntryParser parser(reader, std::move(validator));

    const auto result = parser.parse(0);

    ASSERT_EQ(result.state, fat32tree::DirectoryEntryState::Normal);
    EXPECT_EQ(result.entry.name, "README.TXT");
    EXPECT_EQ(result.entry.type, fat32tree::DirectoryEntryType::File);
    EXPECT_EQ(result.entry.firstCluster, 0x12345678U);
    EXPECT_EQ(result.entry.fileSize, 0x89ABCDEFU);
}

TEST_F(DirectoryEntryParserTest, ParsesDirectoryAndShortNameWithoutExtension)
{
    auto bytes = makeEntry(paddedName("SUBDIR"), fat32tree::fat32::kAttributeDirectory);
    writeU16LE(bytes, fat32tree::fat32::kDirectoryFirstClusterHighOffset, 1);
    writeU16LE(bytes, fat32tree::fat32::kDirectoryFirstClusterLowOffset, 2);
    writeU32LE(bytes, fat32tree::fat32::kDirectoryFileSizeOffset, 0);
    writeImage(bytes);
    fat32tree::BinaryReader reader(m_imagePath);
    auto validator = std::make_unique<fat32tree::test::DirectoryEntryValidatorMock>();
    EXPECT_CALL(*validator, isValidShortName(_)).WillOnce(Return(true));
    fat32tree::DirectoryEntryParser parser(reader, std::move(validator));

    const auto result = parser.parse(0);

    ASSERT_EQ(result.state, fat32tree::DirectoryEntryState::Normal);
    EXPECT_EQ(result.entry.name, "SUBDIR");
    EXPECT_EQ(result.entry.type, fat32tree::DirectoryEntryType::Directory);
    EXPECT_EQ(result.entry.firstCluster, 0x00010002U);
    EXPECT_EQ(result.entry.fileSize, 0U);
}

TEST_F(DirectoryEntryParserTest, ParsesEmptyFileWithZeroClusterAndSize)
{
    auto bytes = makeEntry(shortName("EMPTY", "TXT"));
    writeU16LE(bytes, fat32tree::fat32::kDirectoryFirstClusterHighOffset, 0);
    writeU16LE(bytes, fat32tree::fat32::kDirectoryFirstClusterLowOffset, 0);
    writeU32LE(bytes, fat32tree::fat32::kDirectoryFileSizeOffset, 0);
    writeImage(bytes);
    fat32tree::BinaryReader reader(m_imagePath);
    auto validator = std::make_unique<fat32tree::test::DirectoryEntryValidatorMock>();
    EXPECT_CALL(*validator, isValidShortName(_)).WillOnce(Return(true));
    fat32tree::DirectoryEntryParser parser(reader, std::move(validator));

    const auto result = parser.parse(0);

    ASSERT_EQ(result.state, fat32tree::DirectoryEntryState::Normal);
    EXPECT_EQ(result.entry.name, "EMPTY.TXT");
    EXPECT_EQ(result.entry.type, fat32tree::DirectoryEntryType::File);
    EXPECT_EQ(result.entry.firstCluster, 0U);
    EXPECT_EQ(result.entry.fileSize, 0U);
}

TEST_F(DirectoryEntryParserTest, ParsesDotAndDotDotEntriesAsDirectories)
{
    for (const std::string name : {".", ".."})
    {
        auto bytes = makeEntry(paddedName(name.c_str()), fat32tree::fat32::kAttributeDirectory);
        writeU16LE(bytes, fat32tree::fat32::kDirectoryFirstClusterHighOffset, 0);
        writeU16LE(bytes, fat32tree::fat32::kDirectoryFirstClusterLowOffset, 0);
        writeU32LE(bytes, fat32tree::fat32::kDirectoryFileSizeOffset, 0);
        writeImage(bytes);
        fat32tree::BinaryReader reader(m_imagePath);
        auto validator = std::make_unique<fat32tree::test::DirectoryEntryValidatorMock>();
        EXPECT_CALL(*validator, isValidShortName(_)).WillOnce(Return(true));
        fat32tree::DirectoryEntryParser parser(reader, std::move(validator));

        const auto result = parser.parse(0);

        EXPECT_EQ(result.state, fat32tree::DirectoryEntryState::Normal);
        EXPECT_EQ(result.entry.type, fat32tree::DirectoryEntryType::Directory);
        EXPECT_EQ(result.entry.name, name);
    }
}

TEST_F(DirectoryEntryParserTest, ReturnsInvalidWhenFirstClusterHighWordCannotBeRead)
{
    auto bytes = makeEntry(paddedName("FILE"));
    bytes.resize(fat32tree::fat32::kDirectoryFirstClusterHighOffset);
    writeImage(bytes);
    fat32tree::BinaryReader reader(m_imagePath);
    auto validator = std::make_unique<fat32tree::test::DirectoryEntryValidatorMock>();
    EXPECT_CALL(*validator, isValidShortName(_)).WillOnce(Return(true));
    fat32tree::DirectoryEntryParser parser(reader, std::move(validator));

    const auto result = parser.parse(0);

    EXPECT_EQ(result.state, fat32tree::DirectoryEntryState::Invalid);
}

TEST_F(DirectoryEntryParserTest, ReturnsInvalidWhenFirstClusterLowWordCannotBeRead)
{
    auto bytes = makeEntry(paddedName("FILE"));
    bytes.resize(
        fat32tree::fat32::kDirectoryFirstClusterLowOffset + 1);
    writeImage(bytes);
    fat32tree::BinaryReader reader(m_imagePath);
    auto validator = std::make_unique<fat32tree::test::DirectoryEntryValidatorMock>();
    EXPECT_CALL(*validator, isValidShortName(_)).WillOnce(Return(true));
    fat32tree::DirectoryEntryParser parser(reader, std::move(validator));

    const auto result = parser.parse(0);

    EXPECT_EQ(result.state, fat32tree::DirectoryEntryState::Invalid);
}

TEST_F(DirectoryEntryParserTest, ReturnsInvalidWhenFileSizeCannotBeRead)
{
    auto bytes = makeEntry(paddedName("FILE"));
    bytes.resize(fat32tree::fat32::kDirectoryFileSizeOffset + 3);
    writeImage(bytes);
    fat32tree::BinaryReader reader(m_imagePath);
    auto validator = std::make_unique<fat32tree::test::DirectoryEntryValidatorMock>();
    EXPECT_CALL(*validator, isValidShortName(_)).WillOnce(Return(true));
    fat32tree::DirectoryEntryParser parser(reader, std::move(validator));

    const auto result = parser.parse(0);

    EXPECT_EQ(result.state, fat32tree::DirectoryEntryState::Invalid);
}

} // namespace
