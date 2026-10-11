#pragma once

#include "fat32tree/IDirectoryEntryValidator.h"

#include <gmock/gmock.h>

namespace fat32tree::test
{

class DirectoryEntryValidatorMock final : public IDirectoryEntryValidator
{
public:
    using ShortNameSpan = std::span<const std::uint8_t, fat32::kShortNameLength>;

    MOCK_METHOD(bool, isValidShortName, (ShortNameSpan rawName), (const, override));
};

} // namespace fat32tree::test
