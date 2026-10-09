#include <fat32tree/BinaryReader.h>
#include <fat32tree/BootSectorParser.h>
#include <fat32tree/ClusterChainReader.h>
#include <fat32tree/Fat32Constants.h>
#include <fat32tree/Fat32VolumeValidator.h>
#include <fat32tree/FatTableReader.h>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <vector>

namespace
{

class Fat32ClusterChainIntegrationTest : public ::testing::Test
{
protected:
	static constexpr std::uint32_t kTotalSectors = 70000;
	static constexpr std::uint32_t kFatSizeSectors = 1000;
	static constexpr std::uint16_t kBytesPerSector = 512;
	static constexpr std::uint64_t kVolumeSize =
		static_cast<std::uint64_t>(kTotalSectors) * kBytesPerSector;
	static constexpr std::uint64_t kSelectedFatOffset = 32ULL * kBytesPerSector;

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
				"Fat32Tree-ClusterChainIntegration-" +
				std::to_string(timestamp) + "-" +
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
		m_imagePath = m_temporaryDirectory / "fat32-chain.img";

		std::ofstream image(m_imagePath, std::ios::binary);
		ASSERT_TRUE(image.is_open());

		std::vector<std::uint8_t> bootSector(512, 0);
		writeU16LE(bootSector, fat32tree::fat32::kBytesPerSectorOffset, kBytesPerSector);
		writeU8(bootSector, fat32tree::fat32::kSectorsPerClusterOffset, 1);
		writeU16LE(bootSector, fat32tree::fat32::kReservedSectorCountOffset, 32);
		writeU8(bootSector, fat32tree::fat32::kFatCountOffset, 2);
		writeU16LE(bootSector, fat32tree::fat32::kRootEntryCountOffset, 0);
		writeU16LE(bootSector, fat32tree::fat32::kTotalSectors16Offset, 0);
		writeU16LE(bootSector, fat32tree::fat32::kFatSize16Offset, 0);
		writeU32LE(bootSector, fat32tree::fat32::kTotalSectors32Offset, kTotalSectors);
		writeU32LE(bootSector, fat32tree::fat32::kFatSize32Offset, kFatSizeSectors);
		writeU16LE(bootSector, fat32tree::fat32::kExtendedFlagsOffset, 0);
		writeU16LE(bootSector, fat32tree::fat32::kFileSystemVersionOffset, 0);
		writeU32LE(bootSector, fat32tree::fat32::kRootClusterOffset, 2);
		writeU8(
			bootSector,
			fat32tree::fat32::kBootSignatureOffset,
			fat32tree::fat32::kBootSignatureFirstByte);
		writeU8(
			bootSector,
			fat32tree::fat32::kBootSignatureOffset + 1,
			fat32tree::fat32::kBootSignatureSecondByte);

		image.write(
			reinterpret_cast<const char*>(bootSector.data()),
			static_cast<std::streamsize>(bootSector.size()));
		ASSERT_TRUE(image.good());

		// Write FAT entries at the real byte offsets in the first FAT copy.
		writeFatEntry(image, 20, 21);
		writeFatEntry(image, 21, 40);
		writeFatEntry(image, 40, 41);
		writeFatEntry(image, 41, 0xAFFFFFFF);

		// Extend to the BPB-declared image size without materializing its
		// intervening zero-filled data region.
		image.seekp(static_cast<std::streamoff>(kVolumeSize - 1), std::ios::beg);
		ASSERT_TRUE(image.good());
		image.put('\0');
		ASSERT_TRUE(image.good());
		image.close();
		ASSERT_TRUE(image.good());
	}

	void TearDown() override
	{
		std::error_code error;
		std::filesystem::remove_all(m_temporaryDirectory, error);
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

	static void writeFatEntry(
		std::ofstream& image,
		std::uint32_t clusterNumber,
		std::uint32_t value)
	{
		const std::uint64_t offset = kSelectedFatOffset +
									 static_cast<std::uint64_t>(clusterNumber) * 4;
		image.seekp(static_cast<std::streamoff>(offset), std::ios::beg);
		const std::vector<std::uint8_t> entryBytes{
			static_cast<std::uint8_t>(value & 0xFFU),
			static_cast<std::uint8_t>((value >> 8) & 0xFFU),
			static_cast<std::uint8_t>((value >> 16) & 0xFFU),
			static_cast<std::uint8_t>((value >> 24) & 0xFFU)};
		image.write(
			reinterpret_cast<const char*>(entryBytes.data()),
			static_cast<std::streamsize>(entryBytes.size()));
		ASSERT_TRUE(image.good());
	}

	std::filesystem::path m_temporaryDirectory;
	std::filesystem::path m_imagePath;
};

TEST_F(Fat32ClusterChainIntegrationTest, ReadsFragmentedChainFromSyntheticImage)
{
	fat32tree::BinaryReader reader(m_imagePath);
	ASSERT_EQ(reader.size(), kVolumeSize);

	fat32tree::BootSectorParser bootSectorParser(reader);
	const auto bpb = bootSectorParser.parse();

	const fat32tree::Fat32VolumeValidator validator;
	const auto layout = validator.validate(bpb, reader.size());
	ASSERT_EQ(layout.selectedFatOffset, kSelectedFatOffset);

	fat32tree::FatTableReader fatTableReader(reader, layout);
	const fat32tree::ClusterChainReader chainReader(fatTableReader, layout);

	const auto result = chainReader.getClusterChain(20);

	EXPECT_EQ(result.status, fat32tree::ClusterChainStatus::Complete);
	EXPECT_EQ(result.clusters,
		(std::vector<std::uint32_t>{20, 21, 40, 41}));
}

} // namespace
