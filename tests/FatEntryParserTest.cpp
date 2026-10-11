#include "fat32tree/FatEntryParser.h"

#include <gtest/gtest.h>

namespace
{

TEST(FatEntryParserTest, ParsesFreeEntry)
{
    const fat32tree::FatEntry entry = fat32tree::FatEntryParser{}.parse(0x00000000);

    EXPECT_EQ(entry.type, fat32tree::FatEntryType::Free);
    EXPECT_EQ(entry.value, 0x00000000U);
}

TEST(FatEntryParserTest, MasksReservedHighBitsForFreeEntry)
{
    const fat32tree::FatEntry entry = fat32tree::FatEntryParser{}.parse(0xF0000000);

    EXPECT_EQ(entry.type, fat32tree::FatEntryType::Free);
    EXPECT_EQ(entry.value, 0x00000000U);
}

TEST(FatEntryParserTest, ParsesReservedEntry)
{
    const fat32tree::FatEntry entry = fat32tree::FatEntryParser{}.parse(0x00000001);

    EXPECT_EQ(entry.type, fat32tree::FatEntryType::Reserved);
    EXPECT_EQ(entry.value, 0x00000001U);
}

TEST(FatEntryParserTest, MasksReservedHighBitsForReservedEntry)
{
    const fat32tree::FatEntry entry = fat32tree::FatEntryParser{}.parse(0xA0000001);

    EXPECT_EQ(entry.type, fat32tree::FatEntryType::Reserved);
    EXPECT_EQ(entry.value, 0x00000001U);
}

TEST(FatEntryParserTest, ParsesNextCluster)
{
    const fat32tree::FatEntry entry = fat32tree::FatEntryParser{}.parse(0x00000028);

    EXPECT_EQ(entry.type, fat32tree::FatEntryType::NextCluster);
    EXPECT_EQ(entry.value, 40U);
}

TEST(FatEntryParserTest, MasksReservedHighBitsForNextCluster)
{
    const fat32tree::FatEntry entry = fat32tree::FatEntryParser{}.parse(0xA0000028);

    EXPECT_EQ(entry.type, fat32tree::FatEntryType::NextCluster);
    EXPECT_EQ(entry.value, 40U);
}

TEST(FatEntryParserTest, ParsesLastNextClusterValueBeforeReservedRange)
{
    const fat32tree::FatEntry entry = fat32tree::FatEntryParser{}.parse(0x0FFFFFEF);

    EXPECT_EQ(entry.type, fat32tree::FatEntryType::NextCluster);
    EXPECT_EQ(entry.value, 0x0FFFFFEFU);
}

TEST(FatEntryParserTest, ParsesReservedClusterRangeBoundaries)
{
    const fat32tree::FatEntryParser parser;

    const auto firstReserved = parser.parse(0x0FFFFFF0);
    const auto lastReserved = parser.parse(0x0FFFFFF6);

    EXPECT_EQ(firstReserved.type, fat32tree::FatEntryType::Reserved);
    EXPECT_EQ(firstReserved.value, 0x0FFFFFF0U);
    EXPECT_EQ(lastReserved.type, fat32tree::FatEntryType::Reserved);
    EXPECT_EQ(lastReserved.value, 0x0FFFFFF6U);
}

TEST(FatEntryParserTest, ParsesBadClusterBoundaryWithAndWithoutHighBits)
{
    const fat32tree::FatEntryParser parser;

    const auto exactBoundary = parser.parse(0x0FFFFFF7);
    const auto reservedHighBits = parser.parse(0xAFFFFFF7);

    EXPECT_EQ(exactBoundary.type, fat32tree::FatEntryType::Bad);
    EXPECT_EQ(exactBoundary.value, 0x0FFFFFF7U);
    EXPECT_EQ(reservedHighBits.type, fat32tree::FatEntryType::Bad);
    EXPECT_EQ(reservedHighBits.value, 0x0FFFFFF7U);
}

TEST(FatEntryParserTest, ParsesMinimumEndOfChainValue)
{
    const fat32tree::FatEntry entry = fat32tree::FatEntryParser{}.parse(0x0FFFFFF8);

    EXPECT_EQ(entry.type, fat32tree::FatEntryType::EndOfChain);
    EXPECT_EQ(entry.value, 0x0FFFFFF8U);
}

TEST(FatEntryParserTest, ParsesMaximumEndOfChainValue)
{
    const fat32tree::FatEntry entry = fat32tree::FatEntryParser{}.parse(0x0FFFFFFF);

    EXPECT_EQ(entry.type, fat32tree::FatEntryType::EndOfChain);
    EXPECT_EQ(entry.value, 0x0FFFFFFFU);
}

TEST(FatEntryParserTest, MasksMaximumRawValueToMaximumEndOfChain)
{
    const fat32tree::FatEntry entry = fat32tree::FatEntryParser{}.parse(0xFFFFFFFF);

    EXPECT_EQ(entry.type, fat32tree::FatEntryType::EndOfChain);
    EXPECT_EQ(entry.value, 0x0FFFFFFFU);
}

} // namespace
