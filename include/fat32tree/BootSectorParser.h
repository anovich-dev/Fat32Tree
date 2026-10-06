#pragma once

#include "fat32tree/BinaryReader.h"
#include "fat32tree/Fat32Bpb.h"
#include "fat32tree/ReadResult.h"

#include <stdexcept>
#include <string>

namespace fat32tree
{

class BootSectorParseException : public std::runtime_error
{
public:
    explicit BootSectorParseException(const std::string& message)
        : std::runtime_error(message)
    {
    }
};

class BootSectorParser
{
public:
    explicit BootSectorParser(BinaryReader& reader);

    Fat32Bpb parse();

private:
    template <SupportedReadType T>
    T requireRead(
        ReadResult<T> result,
        const char* fieldName)
    {
        if (result.status != ReadStatus::Success)
        {
            throw BootSectorParseException(
                std::string("Failed to read required field ") +
                fieldName +
                ": " +
                readStatusToString(result.status));
        }

        return result.value;
    }

    BinaryReader& m_reader;
};

} // namespace fat32tree