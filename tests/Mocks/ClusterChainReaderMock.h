#pragma once

#include "fat32tree/IClusterChainReader.h"

#include <gmock/gmock.h>

namespace fat32tree::test
{

class ClusterChainReaderMock : public IClusterChainReader
{
public:
    MOCK_METHOD(
        ClusterChainResult, getClusterChain, (std::uint32_t firstCluster), (const, override));
};

} // namespace fat32tree::test
