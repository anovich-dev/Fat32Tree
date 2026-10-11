#include "fat32tree/core/Fat32Constants.h"
#include "fat32tree/directory/DirectoryEntryValidator.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <string_view>

#include <gtest/gtest.h>

namespace
{

using ShortName = std::array<std::uint8_t, fat32tree::fat32::kShortNameLength>;

ShortName makeShortName(std::string_view base, std::string_view extension = {})
{
    ShortName name{};
    name.fill(static_cast<std::uint8_t>(' '));

    std::copy_n(
        base.begin(), std::min(base.size(), fat32tree::fat32::kShortNameBaseLength), name.begin());
    std::copy_n(extension.begin(),
        std::min(extension.size(), fat32tree::fat32::kShortNameExtensionLength),
        name.begin() + fat32tree::fat32::kShortNameBaseLength);

    return name;
}

class DirectoryEntryValidatorTest : public ::testing::Test
{
protected:
    bool isValid(const ShortName& name) const
    {
        return m_validator.isValidShortName(name);
    }

protected:
    fat32tree::DirectoryEntryValidator m_validator;
};

TEST_F(DirectoryEntryValidatorTest, ValidNormalShortNames)
{
    EXPECT_TRUE(isValid(makeShortName("README", "TXT")));
    EXPECT_TRUE(isValid(makeShortName("FILE", "BIN")));
}

TEST_F(DirectoryEntryValidatorTest, ValidNameWithoutExtension)
{
    EXPECT_TRUE(isValid(makeShortName("README")));
}

TEST_F(DirectoryEntryValidatorTest, RejectsLeadingSpace)
{
    auto name = makeShortName("README", "TXT");
    name[0] = static_cast<std::uint8_t>(' ');

    EXPECT_FALSE(isValid(name));
}

TEST_F(DirectoryEntryValidatorTest, RejectsLowercaseBaseName)
{
    EXPECT_FALSE(isValid(makeShortName("readme", "TXT")));
}

TEST_F(DirectoryEntryValidatorTest, RejectsLowercaseExtension)
{
    EXPECT_FALSE(isValid(makeShortName("README", "txt")));
}

TEST(DirectoryEntryValidatorForbiddenCharacterTest, RejectsForbiddenCharacters)
{
    constexpr std::array<std::uint8_t, 16> forbiddenCharacters{0x22, 0x2A, 0x2B, 0x2C, 0x2E, 0x2F,
        0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x5B, 0x5C, 0x5D, 0x7C};
    const fat32tree::DirectoryEntryValidator validator;

    for (const auto character : forbiddenCharacters)
    {
        auto name = makeShortName("README", "TXT");
        name[2] = character;

        EXPECT_FALSE(validator.isValidShortName(name))
            << "character byte: 0x" << std::hex << static_cast<unsigned>(character);
    }
}

TEST_F(DirectoryEntryValidatorTest, RejectsControlCharacterWithinName)
{
    auto name = makeShortName("README", "TXT");
    name[3] = 0x1F;

    EXPECT_FALSE(isValid(name));
}

TEST_F(DirectoryEntryValidatorTest, RejectsZeroByteWithinName)
{
    auto name = makeShortName("README", "TXT");
    name[4] = 0x00;

    EXPECT_FALSE(isValid(name));
}

TEST_F(DirectoryEntryValidatorTest, AcceptsSpaceWithinNameField)
{
    auto name = makeShortName("READ ME", "TXT");

    EXPECT_TRUE(isValid(name));
}

TEST_F(DirectoryEntryValidatorTest, RejectsControlByteButAcceptsSpaceBoundary)
{
    auto controlName = makeShortName("README", "TXT");
    controlName[6] = 0x1F;
    auto spaceName = makeShortName("README", "TXT");
    spaceName[6] = 0x20;

    EXPECT_FALSE(isValid(controlName));
    EXPECT_TRUE(isValid(spaceName));
}

TEST_F(DirectoryEntryValidatorTest, AcceptsDotEntry)
{
    ShortName name{};
    name.fill(static_cast<std::uint8_t>(' '));
    name[0] = static_cast<std::uint8_t>('.');

    EXPECT_TRUE(isValid(name));
}

TEST_F(DirectoryEntryValidatorTest, AcceptsDotDotEntry)
{
    ShortName name{};
    name.fill(static_cast<std::uint8_t>(' '));
    name[0] = static_cast<std::uint8_t>('.');
    name[1] = static_cast<std::uint8_t>('.');

    EXPECT_TRUE(isValid(name));
}

TEST_F(DirectoryEntryValidatorTest, RejectsMalformedDotEntries)
{
    ShortName dotAndSuffix{};
    dotAndSuffix.fill(static_cast<std::uint8_t>(' '));
    dotAndSuffix[0] = static_cast<std::uint8_t>('.');
    dotAndSuffix[1] = static_cast<std::uint8_t>('A');
    dotAndSuffix[2] = static_cast<std::uint8_t>('B');
    dotAndSuffix[3] = static_cast<std::uint8_t>('C');

    ShortName dotDotAndSuffix{};
    dotDotAndSuffix.fill(static_cast<std::uint8_t>(' '));
    dotDotAndSuffix[0] = static_cast<std::uint8_t>('.');
    dotDotAndSuffix[1] = static_cast<std::uint8_t>('.');
    dotDotAndSuffix[2] = static_cast<std::uint8_t>('A');
    dotDotAndSuffix[3] = static_cast<std::uint8_t>('B');
    dotDotAndSuffix[4] = static_cast<std::uint8_t>('C');

    EXPECT_FALSE(isValid(dotAndSuffix));
    EXPECT_FALSE(isValid(dotDotAndSuffix));
}

TEST_F(DirectoryEntryValidatorTest, AcceptsKanjiMarkerAtFirstByte)
{
    auto name = makeShortName("README", "TXT");
    name[0] = fat32tree::fat32::kDirectoryEntryKanjiMarker;

    EXPECT_TRUE(isValid(name));
}

TEST_F(DirectoryEntryValidatorTest, RejectsKanjiMarkerAfterFirstByte)
{
    for (std::size_t index = 1; index < fat32tree::fat32::kShortNameLength; ++index)
    {
        auto name = makeShortName("README", "TXT");
        name[index] = fat32tree::fat32::kDirectoryEntryKanjiMarker;

        EXPECT_FALSE(isValid(name)) << "index: " << index;
    }
}

TEST_F(DirectoryEntryValidatorTest, AcceptsNonAsciiOemBytes)
{
    auto name = makeShortName("README", "TXT");
    name[1] = 0x80;
    name[fat32tree::fat32::kShortNameBaseLength] = 0xE1;

    EXPECT_TRUE(isValid(name));
}

} // namespace
