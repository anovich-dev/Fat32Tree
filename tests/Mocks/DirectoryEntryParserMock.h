#pragma once

#include <gmock/gmock.h>

#include "fat32tree/IDirectoryEntryParser.h"

namespace fat32tree::test
{

class DirectoryEntryParserMock
    : public IDirectoryEntryParser
{
public:
    MOCK_METHOD(
        ParsedDirectoryEntry,
        parse,
        (std::uint64_t entryOffset),
        (const, override));
};

} // namespace fat32tree::test