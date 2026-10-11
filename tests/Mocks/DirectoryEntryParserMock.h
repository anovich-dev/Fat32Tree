#pragma once

#include "fat32tree/directory/IDirectoryEntryParser.h"

#include <gmock/gmock.h>

namespace fat32tree::test
{

class DirectoryEntryParserMock : public IDirectoryEntryParser
{
public:
    MOCK_METHOD(ParsedDirectoryEntry, parse, (std::uint64_t entryOffset), (const, override));
};

} // namespace fat32tree::test
