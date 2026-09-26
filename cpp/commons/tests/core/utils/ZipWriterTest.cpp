#include <gtest/gtest.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <random>
#include <stdexcept>

#include "aion/commons/utils/ZipWriter.h"

using namespace aion::commons::utils;

namespace {

/** A straightforward DEFLATE decoder (after zlib's puff.c) supporting all block types, to validate the encoder. */
class Inflater {
public:
	explicit Inflater(std::span<const uint8_t> input) : input(input) {}

	std::vector<uint8_t> inflate() {
		bool last;
		do {
			last = bits(1) == 1;
			int type = bits(2);
			if (type == 0)
				stored();
			else if (type == 1)
				fixed();
			else if (type == 2)
				dynamic();
			else
				throw std::runtime_error("invalid block type");
		} while (!last);
		return output;
	}

private:
	struct Huffman {
		std::array<uint16_t, 16> count{};
		std::vector<uint16_t> symbol;
	};

	int bits(int need) {
		uint64_t value = bitBuffer;
		while (bitCount < need) {
			if (position >= input.size())
				throw std::runtime_error("out of input");
			value |= static_cast<uint64_t>(input[position++]) << bitCount;
			bitCount += 8;
		}
		bitBuffer = value >> need;
		bitCount -= need;
		return static_cast<int>(value & ((1ULL << need) - 1));
	}

	void stored() {
		bitBuffer = 0;
		bitCount = 0;
		if (position + 4 > input.size())
			throw std::runtime_error("out of input");
		uint32_t length = input[position] | (input[position + 1] << 8);
		position += 4;
		if (position + length > input.size())
			throw std::runtime_error("out of input");
		output.insert(output.end(), input.begin() + position, input.begin() + position + length);
		position += length;
	}

	static Huffman build(const uint16_t* lengths, size_t n) {
		Huffman h;
		h.symbol.resize(n);
		for (size_t i = 0; i < n; i++)
			h.count[lengths[i]]++;
		std::array<uint16_t, 16> offsets{};
		for (size_t len = 1; len < 15; len++)
			offsets[len + 1] = static_cast<uint16_t>(offsets[len] + h.count[len]);
		for (size_t i = 0; i < n; i++) {
			if (lengths[i] != 0)
				h.symbol[offsets[lengths[i]]++] = static_cast<uint16_t>(i);
		}
		return h;
	}

	int decode(const Huffman& h) {
		int code = 0;
		int first = 0;
		int index = 0;
		for (int len = 1; len <= 15; len++) {
			code |= bits(1);
			int count = h.count[static_cast<size_t>(len)];
			if (code - count < first)
				return h.symbol[static_cast<size_t>(index + (code - first))];
			index += count;
			first += count;
			first <<= 1;
			code <<= 1;
		}
		throw std::runtime_error("invalid code");
	}

	void codes(const Huffman& lengthCodes, const Huffman& distanceCodes) {
		static constexpr uint16_t LENGTH_BASE[] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
		static constexpr uint16_t LENGTH_EXTRA[] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
		static constexpr uint16_t DISTANCE_BASE[] = {1,   2,   3,   4,   5,   7,    9,    13,   17,   25,   33,   49,   65,    97,    129,
																								 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
		static constexpr uint16_t DISTANCE_EXTRA[] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
		while (true) {
			int symbol = decode(lengthCodes);
			if (symbol < 256) {
				output.push_back(static_cast<uint8_t>(symbol));
			} else if (symbol == 256) {
				return;
			} else {
				symbol -= 257;
				if (symbol >= 29)
					throw std::runtime_error("invalid length symbol");
				size_t length = LENGTH_BASE[symbol] + static_cast<size_t>(bits(LENGTH_EXTRA[symbol]));
				int distanceSymbol = decode(distanceCodes);
				if (distanceSymbol >= 30)
					throw std::runtime_error("invalid distance symbol");
				size_t distance = DISTANCE_BASE[distanceSymbol] + static_cast<size_t>(bits(DISTANCE_EXTRA[distanceSymbol]));
				if (distance > output.size() || distance > 32768)
					throw std::runtime_error("distance too far back");
				for (size_t i = 0; i < length; i++)
					output.push_back(output[output.size() - distance]);
			}
		}
	}

	void fixed() {
		std::array<uint16_t, 288> lengths{};
		for (size_t i = 0; i < 288; i++)
			lengths[i] = i < 144 ? 8 : i < 256 ? 9 : i < 280 ? 7 : 8;
		std::array<uint16_t, 30> distanceLengths{};
		distanceLengths.fill(5);
		codes(build(lengths.data(), lengths.size()), build(distanceLengths.data(), distanceLengths.size()));
	}

	void dynamic() {
		static constexpr size_t ORDER[] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
		size_t literalCount = static_cast<size_t>(bits(5)) + 257;
		size_t distanceCount = static_cast<size_t>(bits(5)) + 1;
		size_t codeCount = static_cast<size_t>(bits(4)) + 4;
		std::array<uint16_t, 320> lengths{};
		for (size_t i = 0; i < codeCount; i++)
			lengths[ORDER[i]] = static_cast<uint16_t>(bits(3));
		Huffman lengthCode = build(lengths.data(), 19);
		lengths.fill(0);
		for (size_t index = 0; index < literalCount + distanceCount;) {
			int symbol = decode(lengthCode);
			if (symbol < 16) {
				lengths[index++] = static_cast<uint16_t>(symbol);
			} else {
				uint16_t value = 0;
				size_t repeat;
				if (symbol == 16) {
					value = lengths[index - 1];
					repeat = 3 + static_cast<size_t>(bits(2));
				} else if (symbol == 17) {
					repeat = 3 + static_cast<size_t>(bits(3));
				} else {
					repeat = 11 + static_cast<size_t>(bits(7));
				}
				while (repeat-- > 0)
					lengths[index++] = value;
			}
		}
		codes(build(lengths.data(), literalCount), build(lengths.data() + literalCount, distanceCount));
	}

	std::span<const uint8_t> input;
	size_t position = 0;
	uint64_t bitBuffer = 0;
	int bitCount = 0;
	std::vector<uint8_t> output;
};

std::vector<uint8_t> deflate(std::span<const uint8_t> data, size_t chunkSize = SIZE_MAX) {
	std::vector<uint8_t> compressed;
	DeflateEncoder encoder([&](std::span<const uint8_t> chunk) { compressed.insert(compressed.end(), chunk.begin(), chunk.end()); });
	for (size_t offset = 0; offset < data.size(); offset += std::min(chunkSize, data.size() - offset))
		encoder.write(data.subspan(offset, std::min(chunkSize, data.size() - offset)));
	encoder.finish();
	return compressed;
}

std::vector<uint8_t> bytesOf(std::string_view text) {
	return {text.begin(), text.end()};
}

std::vector<uint8_t> logLikeText(size_t lines) {
	std::string text;
	std::mt19937 random(42);
	for (size_t i = 0; i < lines; i++) {
		text += "2026-09-12T15:42:" + std::to_string(i % 60) + "," + std::to_string(random() % 1000) + "+02:00 INFO  [InstantPool-" +
			std::to_string(random() % 8) + "] com.aionemu.gameserver.services.SomeService - Player " + std::to_string(random() % 5000) +
			" did something\r\n";
	}
	return bytesOf(text);
}

uint16_t read16(const std::vector<uint8_t>& data, size_t offset) {
	return static_cast<uint16_t>(data[offset] | (data[offset + 1] << 8));
}

uint32_t read32(const std::vector<uint8_t>& data, size_t offset) {
	return read16(data, offset) | (static_cast<uint32_t>(read16(data, offset + 2)) << 16);
}

} // namespace

TEST(ZipWriterTest, Crc32) {
	Crc32 crc;
	auto data = bytesOf("123456789");
	crc.update(data);
	EXPECT_EQ(crc.getValue(), 0xCBF43926u); // standard check value
	crc.reset();
	EXPECT_EQ(crc.getValue(), 0u);
}

TEST(ZipWriterTest, DeflateRoundTrips) {
	EXPECT_TRUE(Inflater(deflate({})).inflate().empty());

	auto hello = bytesOf("hello hello hello hello, world");
	EXPECT_EQ(Inflater(deflate(hello)).inflate(), hello);

	std::vector<uint8_t> random(200'000);
	std::mt19937 generator(1);
	for (auto& b : random)
		b = static_cast<uint8_t>(generator());
	EXPECT_EQ(Inflater(deflate(random)).inflate(), random);

	std::vector<uint8_t> zeros(1'000'000, 0);
	auto compressedZeros = deflate(zeros);
	EXPECT_EQ(Inflater(compressedZeros).inflate(), zeros);
	EXPECT_LT(compressedZeros.size(), 10000u);
}

TEST(ZipWriterTest, DeflateCompressesLogsAcrossChunks) {
	auto text = logLikeText(20'000); // about 2.4 MB
	auto oneChunk = deflate(text);
	EXPECT_EQ(Inflater(oneChunk).inflate(), text);
	EXPECT_LT(oneChunk.size(), text.size() / 4) << "compressed to " << oneChunk.size() << " of " << text.size();

	auto smallChunks = deflate(text, 777);
	EXPECT_EQ(Inflater(smallChunks).inflate(), text);

	auto prefix = std::span<const uint8_t>(text).first(5000);
	auto byteChunks = deflate(prefix, 1);
	EXPECT_EQ(Inflater(byteChunks).inflate(), std::vector<uint8_t>(prefix.begin(), prefix.end()));
}

TEST(ZipWriterTest, DeflateMaximumDistance) {
	std::vector<uint8_t> data(32768 + 300);
	std::mt19937 generator(7);
	for (size_t i = 0; i < 300; i++)
		data[i] = static_cast<uint8_t>(generator());
	for (size_t i = 300; i < 32768; i++)
		data[i] = static_cast<uint8_t>(i * 7 % 251);
	std::copy_n(data.begin(), 300, data.begin() + 32768); // repeats the random start exactly one window later
	EXPECT_EQ(Inflater(deflate(data)).inflate(), data);
}

TEST(ZipWriterTest, WritesReadableArchive) {
	auto folder = std::filesystem::temp_directory_path() / "aion_ZipWriterTest";
	std::filesystem::remove_all(folder);
	std::filesystem::create_directories(folder);
	auto source = folder / "server_console.log";
	auto text = logLikeText(1000);
	std::ofstream(source, std::ios::binary).write(reinterpret_cast<const char*>(text.data()), static_cast<std::streamsize>(text.size()));

	auto zipFile = folder / "archive.zip";
	{
		ZipWriter zip(zipFile);
		zip.addFile("server_console.log", source);
		zip.addEntry("stats/MethodStats.log", bytesOf("<entries/>"), std::chrono::system_clock::now());
		zip.addEntry("empty.log", {}, std::chrono::system_clock::now());
		zip.finish();
	}

	std::ifstream in(zipFile, std::ios::binary);
	std::vector<uint8_t> zip((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	in.close();
	ASSERT_GE(zip.size(), 22u);
	size_t eocd = zip.size() - 22;
	ASSERT_EQ(read32(zip, eocd), 0x06054B50u);
	ASSERT_EQ(read16(zip, eocd + 10), 3u);
	size_t directory = read32(zip, eocd + 16);
	EXPECT_EQ(directory + read32(zip, eocd + 12), eocd);

	std::vector<std::pair<std::string, std::vector<uint8_t>>> expected = {
		{"server_console.log", text}, {"stats/MethodStats.log", bytesOf("<entries/>")}, {"empty.log", {}}};
	size_t offset = directory;
	for (const auto& [name, contents] : expected) {
		ASSERT_EQ(read32(zip, offset), 0x02014B50u);
		uint32_t crc = read32(zip, offset + 16);
		uint32_t compressedSize = read32(zip, offset + 20);
		uint32_t size = read32(zip, offset + 24);
		uint16_t nameLength = read16(zip, offset + 28);
		uint32_t localHeader = read32(zip, offset + 42);
		EXPECT_EQ(std::string(zip.begin() + offset + 46, zip.begin() + offset + 46 + nameLength), name);
		EXPECT_EQ(size, contents.size());

		ASSERT_EQ(read32(zip, localHeader), 0x04034B50u);
		EXPECT_EQ(read16(zip, localHeader + 8), 8u); // deflated
		EXPECT_EQ(read32(zip, localHeader + 14), crc);
		EXPECT_EQ(read32(zip, localHeader + 18), compressedSize);
		size_t dataStart = localHeader + 30 + read16(zip, localHeader + 26) + read16(zip, localHeader + 28);
		auto data = std::span<const uint8_t>(zip).subspan(dataStart, compressedSize);
		EXPECT_EQ(Inflater(data).inflate(), contents) << name;
		Crc32 check;
		check.update(contents);
		EXPECT_EQ(check.getValue(), crc);

		offset += 46 + nameLength + read16(zip, offset + 30) + read16(zip, offset + 32);
	}
	std::filesystem::remove_all(folder);
}

TEST(ZipWriterTest, ModificationTimesAreClampedToTheDosDateRange) {
	using namespace std::chrono;
	auto folder = std::filesystem::temp_directory_path() / "aion_ZipWriterTest_dates";
	std::filesystem::remove_all(folder);
	std::filesystem::create_directories(folder);
	auto zipFile = folder / "dates.zip";
	{
		ZipWriter zip(zipFile);
		zip.addEntry("future.log", bytesOf("x"), time_point_cast<system_clock::duration>(sys_days(2200y / June / 15)));
		zip.addEntry("past.log", bytesOf("y"), time_point_cast<system_clock::duration>(sys_days(1970y / June / 15)));
		zip.finish();
	}
	std::ifstream in(zipFile, std::ios::binary);
	std::vector<uint8_t> zip((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	in.close();
	ASSERT_GE(zip.size(), 30u);
	ASSERT_EQ(read32(zip, 0), 0x04034B50u);
	EXPECT_EQ(read16(zip, 10), (23 << 11) | (59 << 5) | 29); // 23:59:58
	EXPECT_EQ(read16(zip, 12), (127 << 9) | (12 << 5) | 31); // 2107-12-31
	size_t second = 30 + read16(zip, 26) + read16(zip, 28) + read32(zip, 18);
	ASSERT_EQ(read32(zip, second), 0x04034B50u);
	EXPECT_EQ(read16(zip, second + 10), 0);                // 00:00:00
	EXPECT_EQ(read16(zip, second + 12), (1 << 5) | 1);     // 1980-01-01
	std::filesystem::remove_all(folder);
}
