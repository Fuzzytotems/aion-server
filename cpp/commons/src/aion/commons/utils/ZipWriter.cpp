#include "aion/commons/utils/ZipWriter.h"

#include <algorithm>
#include <limits>

#include "aion/commons/utils/DateTimeFormatter.h"
#include "aion/commons/utils/Exception.h"

namespace aion::commons::utils {

namespace {

constexpr int32_t WINDOW_SIZE = 32768;
constexpr int32_t MIN_MATCH = 3;
constexpr int32_t MAX_MATCH = 258;
/** bytes kept ahead of the encoding position (unless flushing), so that matches can reach their maximum length */
constexpr int32_t MIN_LOOKAHEAD = MAX_MATCH + MIN_MATCH;
constexpr int32_t HASH_BITS = 15;
constexpr int32_t HASH_SIZE = 1 << HASH_BITS;
/** maximum number of hash chain entries examined per position (zlib level 9 uses 4096, level 6 uses 128) */
constexpr int32_t MAX_CHAIN = 512;
/** matches at least this long are taken without looking further */
constexpr int32_t NICE_LENGTH = 258;
/** matches shorter than this are compared with the match at the next position (lazy matching) */
constexpr int32_t LAZY_LENGTH = 32;
constexpr int32_t END_OF_BLOCK = 256;
constexpr size_t OUTPUT_CHUNK = 1 << 16;

constexpr std::array<uint16_t, 29> LENGTH_BASE = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
constexpr std::array<uint8_t, 29> LENGTH_EXTRA_BITS = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
constexpr std::array<uint16_t, 30> DISTANCE_BASE = {1,   2,   3,   4,   5,   7,    9,    13,   17,   25,   33,   49,   65,    97,    129,
																										193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
constexpr std::array<uint8_t, 30> DISTANCE_EXTRA_BITS = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

constexpr std::array<uint32_t, 256> CRC_TABLE = [] {
	std::array<uint32_t, 256> table{};
	for (uint32_t n = 0; n < 256; n++) {
		uint32_t c = n;
		for (int k = 0; k < 8; k++)
			c = (c & 1) ? 0xEDB88320U ^ (c >> 1) : c >> 1;
		table[n] = c;
	}
	return table;
}();

uint32_t reverseBits(uint32_t code, int32_t length) noexcept {
	uint32_t reversed = 0;
	for (int32_t i = 0; i < length; i++) {
		reversed = (reversed << 1) | (code & 1);
		code >>= 1;
	}
	return reversed;
}

void put16(std::vector<uint8_t>& out, uint32_t value) {
	out.push_back(static_cast<uint8_t>(value));
	out.push_back(static_cast<uint8_t>(value >> 8));
}

void put32(std::vector<uint8_t>& out, uint32_t value) {
	put16(out, value & 0xFFFF);
	put16(out, value >> 16);
}

} // namespace

void Crc32::update(std::span<const uint8_t> data) noexcept {
	for (uint8_t b : data)
		crc = CRC_TABLE[(crc ^ b) & 0xFF] ^ (crc >> 8);
}

DeflateEncoder::DeflateEncoder(Output output) : output(std::move(output)), head(HASH_SIZE, -1), prev(WINDOW_SIZE, -1) {
	writeBits(0, 1); // BFINAL: the data block is followed by an empty final block, since the end of the data is not known yet
	writeBits(1, 2); // BTYPE: fixed Huffman codes
}

void DeflateEncoder::write(std::span<const uint8_t> data) {
	if (finished)
		throw IllegalStateException("DeflateEncoder is finished");
	window.insert(window.end(), data.begin(), data.end());
	process(false);
}

void DeflateEncoder::finish() {
	if (finished)
		return;
	process(true);
	writeHuffman(0, 7); // end of block (symbol 256 has the 7 bit code 0)
	writeBits(1, 1);    // BFINAL
	writeBits(1, 2);    // BTYPE: fixed Huffman codes
	writeHuffman(0, 7); // end of block
	if (bitCount > 0)
		writeBits(0, 8 - bitCount);
	flushOutput(true);
	finished = true;
	window = {};
}

void DeflateEncoder::insertHash(int64_t pos) noexcept {
	const uint8_t* bytes = window.data() + (pos - base);
	uint32_t hash = ((static_cast<uint32_t>(bytes[0]) << 10) ^ (static_cast<uint32_t>(bytes[1]) << 5) ^ bytes[2]) & (HASH_SIZE - 1);
	prev[static_cast<size_t>(pos & (WINDOW_SIZE - 1))] = head[hash];
	head[hash] = pos;
}

int32_t DeflateEncoder::findMatch(int64_t pos, int32_t& matchDistance) const noexcept {
	int64_t end = base + static_cast<int64_t>(window.size());
	int32_t maxLength = static_cast<int32_t>(std::min<int64_t>(MAX_MATCH, end - pos));
	if (maxLength < MIN_MATCH)
		return 0;
	const uint8_t* current = window.data() + (pos - base);
	uint32_t hash = ((static_cast<uint32_t>(current[0]) << 10) ^ (static_cast<uint32_t>(current[1]) << 5) ^ current[2]) & (HASH_SIZE - 1);
	int32_t bestLength = 0;
	int64_t candidate = head[hash];
	int64_t previousCandidate = pos;
	for (int32_t chain = 0; chain < MAX_CHAIN && candidate >= 0; chain++) {
		// stale chain entries (overwritten slots) are detected by candidates that do not decrease or left the window
		if (candidate >= previousCandidate || pos - candidate > WINDOW_SIZE || candidate < base)
			break;
		const uint8_t* match = window.data() + (candidate - base);
		if (match[bestLength] == current[bestLength]) {
			int32_t length = 0;
			while (length < maxLength && match[length] == current[length])
				length++;
			if (length > bestLength) {
				bestLength = length;
				matchDistance = static_cast<int32_t>(pos - candidate);
				if (length >= NICE_LENGTH || length == maxLength)
					break;
			}
		}
		previousCandidate = candidate;
		candidate = prev[static_cast<size_t>(candidate & (WINDOW_SIZE - 1))];
	}
	return bestLength >= MIN_MATCH ? bestLength : 0;
}

void DeflateEncoder::process(bool flush) {
	int64_t end = base + static_cast<int64_t>(window.size());
	int64_t hashed = position; // positions below are in the hash chains
	auto hashUpTo = [&](int64_t limit) {
		for (; hashed < limit && hashed + MIN_MATCH <= end; hashed++)
			insertHash(hashed);
		hashed = std::max(hashed, limit);
	};
	while (position < end && (flush || end - position >= MIN_LOOKAHEAD)) {
		int32_t distance = 0;
		int32_t length = findMatch(position, distance);
		if (length > 0 && length < LAZY_LENGTH) {
			hashUpTo(position + 1);
			int32_t nextDistance = 0;
			int32_t nextLength = findMatch(position + 1, nextDistance);
			if (nextLength > length) {
				emitLiteral(window[static_cast<size_t>(position - base)]);
				position++;
				continue;
			}
		}
		if (length > 0) {
			emitMatch(length, distance);
			hashUpTo(position + length);
			position += length;
		} else {
			hashUpTo(position + 1);
			emitLiteral(window[static_cast<size_t>(position - base)]);
			position++;
		}
	}
	// keep one window of history before the current position
	int64_t discard = position - WINDOW_SIZE - base;
	if (discard >= WINDOW_SIZE) {
		window.erase(window.begin(), window.begin() + static_cast<ptrdiff_t>(discard));
		base += discard;
	}
	flushOutput(false);
}

void DeflateEncoder::emitLiteral(uint8_t literal) {
	if (literal <= 143)
		writeHuffman(0x30 + literal, 8);
	else
		writeHuffman(0x190 + (literal - 144U), 9);
}

void DeflateEncoder::emitMatch(int32_t length, int32_t distance) {
	size_t lengthIndex = static_cast<size_t>(std::upper_bound(LENGTH_BASE.begin(), LENGTH_BASE.end(), length) - LENGTH_BASE.begin() - 1);
	uint32_t symbol = 257 + static_cast<uint32_t>(lengthIndex);
	if (symbol <= 279)
		writeHuffman(symbol - 256, 7);
	else
		writeHuffman(0xC0 + (symbol - 280), 8);
	writeBits(static_cast<uint32_t>(length - LENGTH_BASE[lengthIndex]), LENGTH_EXTRA_BITS[lengthIndex]);

	size_t distanceIndex = static_cast<size_t>(std::upper_bound(DISTANCE_BASE.begin(), DISTANCE_BASE.end(), distance) - DISTANCE_BASE.begin() - 1);
	writeHuffman(static_cast<uint32_t>(distanceIndex), 5);
	writeBits(static_cast<uint32_t>(distance - DISTANCE_BASE[distanceIndex]), DISTANCE_EXTRA_BITS[distanceIndex]);
}

void DeflateEncoder::writeBits(uint32_t value, int32_t count) {
	bitBuffer |= static_cast<uint64_t>(value) << bitCount;
	bitCount += count;
	while (bitCount >= 8) {
		pending.push_back(static_cast<uint8_t>(bitBuffer));
		bitBuffer >>= 8;
		bitCount -= 8;
	}
}

void DeflateEncoder::writeHuffman(uint32_t code, int32_t length) {
	writeBits(reverseBits(code, length), length); // Huffman codes are packed starting with the most significant bit
}

void DeflateEncoder::flushOutput(bool force) {
	if (!pending.empty() && (force || pending.size() >= OUTPUT_CHUNK)) {
		output(pending);
		pending.clear();
	}
}

ZipWriter::ZipWriter(const std::filesystem::path& file) : file(file), out(file, std::ios::binary | std::ios::trunc) {
	if (!out)
		throw IOException("Could not create " + file.string());
}

ZipWriter::~ZipWriter() {
	if (!finished) {
		try {
			finish();
		} catch (...) {
			// destructors must not throw; call finish() to handle errors
		}
	}
}

void ZipWriter::checkStream(std::string_view action) {
	if (!out)
		throw IOException("Could not " + std::string(action) + " " + file.string());
}

void ZipWriter::addFile(std::string_view entryName, const std::filesystem::path& source) {
	std::ifstream in(source, std::ios::binary);
	if (!in)
		throw IOException("Could not open " + source.string());
	auto modificationTime = std::chrono::clock_cast<std::chrono::system_clock>(std::filesystem::last_write_time(source));
	addEntry(entryName, std::chrono::time_point_cast<std::chrono::system_clock::duration>(modificationTime), [&](std::span<uint8_t> buffer) -> size_t {
		in.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
		if (in.bad())
			throw IOException("Could not read " + source.string());
		return static_cast<size_t>(in.gcount());
	});
}

void ZipWriter::addEntry(std::string_view entryName, std::span<const uint8_t> contents, std::chrono::system_clock::time_point modificationTime) {
	size_t offset = 0;
	addEntry(entryName, modificationTime, [&](std::span<uint8_t> buffer) -> size_t {
		size_t count = std::min(buffer.size(), contents.size() - offset);
		std::copy_n(contents.begin() + static_cast<ptrdiff_t>(offset), count, buffer.begin());
		offset += count;
		return count;
	});
}

void ZipWriter::addEntry(std::string_view entryName, std::chrono::system_clock::time_point modificationTime,
	const std::function<size_t(std::span<uint8_t>)>& read) {
	if (finished)
		throw IllegalStateException("ZipWriter is finished");
	if (entries.size() >= 0xFFFF || entryName.size() > 0xFFFF)
		throw IOException("Too many entries or entry name too long for " + file.string());

	EntryInfo entry;
	entry.name = entryName;
	DateTimeFields local = DateTimeFields::of(modificationTime, nullptr);
	if (local.year < 1980) // the range of MS-DOS dates is 1980 to 2107
		local = DateTimeFields{.year = 1980, .month = 1, .day = 1};
	else if (local.year > 2107)
		local = DateTimeFields{.year = 2107, .month = 12, .day = 31, .hour = 23, .minute = 59, .second = 59};
	entry.dosDate = static_cast<uint16_t>(((local.year - 1980) << 9) | (local.month << 5) | local.day);
	entry.dosTime = static_cast<uint16_t>((local.hour << 11) | (local.minute << 5) | (local.second / 2));
	entry.localHeaderOffset = static_cast<uint64_t>(out.tellp());

	std::vector<uint8_t> header;
	put32(header, 0x04034B50);
	put16(header, 20);     // version needed to extract: 2.0 (deflate)
	put16(header, 0x0800); // flags: UTF-8 names
	put16(header, 8);      // compression method: deflated
	put16(header, entry.dosTime);
	put16(header, entry.dosDate);
	put32(header, 0); // CRC-32, sizes: written after the data
	put32(header, 0);
	put32(header, 0);
	put16(header, static_cast<uint32_t>(entry.name.size()));
	put16(header, 0); // extra field length
	header.insert(header.end(), entry.name.begin(), entry.name.end());
	out.write(reinterpret_cast<const char*>(header.data()), static_cast<std::streamsize>(header.size()));
	checkStream("write");

	Crc32 crc;
	DeflateEncoder encoder([&](std::span<const uint8_t> data) {
		out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
		entry.compressedSize += data.size();
	});
	std::vector<uint8_t> buffer(OUTPUT_CHUNK);
	while (true) {
		size_t count = read(buffer);
		if (count == 0)
			break;
		std::span<const uint8_t> data(buffer.data(), count);
		crc.update(data);
		entry.uncompressedSize += count;
		encoder.write(data);
		checkStream("write");
	}
	encoder.finish();
	checkStream("write");
	entry.crc = crc.getValue();

	auto dataEnd = out.tellp();
	constexpr uint64_t LIMIT = std::numeric_limits<uint32_t>::max();
	if (entry.compressedSize >= LIMIT || entry.uncompressedSize >= LIMIT || static_cast<uint64_t>(dataEnd) >= LIMIT)
		throw IOException("ZIP64 archives are not supported: " + file.string());
	std::vector<uint8_t> sizes;
	put32(sizes, entry.crc);
	put32(sizes, static_cast<uint32_t>(entry.compressedSize));
	put32(sizes, static_cast<uint32_t>(entry.uncompressedSize));
	out.seekp(static_cast<std::streamoff>(entry.localHeaderOffset + 14));
	out.write(reinterpret_cast<const char*>(sizes.data()), static_cast<std::streamsize>(sizes.size()));
	out.seekp(dataEnd);
	checkStream("write");
	entries.push_back(std::move(entry));
}

void ZipWriter::finish() {
	if (finished)
		return;
	finished = true;
	uint64_t directoryOffset = static_cast<uint64_t>(out.tellp());
	std::vector<uint8_t> directory;
	for (const EntryInfo& entry : entries) {
		put32(directory, 0x02014B50);
		put16(directory, 20);     // version made by: 2.0, MS-DOS attributes
		put16(directory, 20);     // version needed to extract
		put16(directory, 0x0800); // flags: UTF-8 names
		put16(directory, 8);      // deflated
		put16(directory, entry.dosTime);
		put16(directory, entry.dosDate);
		put32(directory, entry.crc);
		put32(directory, static_cast<uint32_t>(entry.compressedSize));
		put32(directory, static_cast<uint32_t>(entry.uncompressedSize));
		put16(directory, static_cast<uint32_t>(entry.name.size()));
		put16(directory, 0); // extra field length
		put16(directory, 0); // comment length
		put16(directory, 0); // disk number start
		put16(directory, 0); // internal attributes
		put32(directory, 0); // external attributes
		put32(directory, static_cast<uint32_t>(entry.localHeaderOffset));
		directory.insert(directory.end(), entry.name.begin(), entry.name.end());
	}
	uint64_t directorySize = directory.size();
	if (directoryOffset + directorySize >= std::numeric_limits<uint32_t>::max())
		throw IOException("ZIP64 archives are not supported: " + file.string());
	put32(directory, 0x06054B50); // end of central directory record
	put16(directory, 0);          // number of this disk
	put16(directory, 0);          // disk where the central directory starts
	put16(directory, static_cast<uint32_t>(entries.size()));
	put16(directory, static_cast<uint32_t>(entries.size()));
	put32(directory, static_cast<uint32_t>(directorySize));
	put32(directory, static_cast<uint32_t>(directoryOffset));
	put16(directory, 0); // comment length
	out.write(reinterpret_cast<const char*>(directory.data()), static_cast<std::streamsize>(directory.size()));
	out.close();
	if (!out)
		throw IOException("Could not write " + file.string());
}

} // namespace aion::commons::utils
