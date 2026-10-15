#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace aion::commons::utils {

/** Java: java.util.zip.CRC32 */
class Crc32 {
public:
	void update(std::span<const uint8_t> data) noexcept;
	uint32_t getValue() const noexcept { return ~crc; }
	void reset() noexcept { crc = 0xFFFFFFFF; }

private:
	uint32_t crc = 0xFFFFFFFF;
};

/**
 * Java: java.util.zip.Deflater - a streaming DEFLATE (RFC 1951) compressor, producing raw deflate data without zlib header. It uses LZ77 with
 * hash chains, lazy matching and the fixed Huffman codes, which compresses text like log files well, though somewhat less than zlib's dynamic
 * Huffman codes (commons cannot link zlib).
 */
class DeflateEncoder {
public:
	/** Receives the compressed data, in chunks. */
	using Output = std::function<void(std::span<const uint8_t>)>;

	explicit DeflateEncoder(Output output);

	/** Compresses the data. Output may be delayed until more data is written or finish() is called. */
	void write(std::span<const uint8_t> data);

	/** Compresses the remaining data and ends the stream. No data may be written afterwards. */
	void finish();

private:
	void process(bool flush);
	void insertHash(int64_t position) noexcept;
	/** @return the best match length at the position (0 if below the minimum) and sets matchDistance */
	int32_t findMatch(int64_t position, int32_t& matchDistance) const noexcept;
	void emitLiteral(uint8_t literal);
	void emitMatch(int32_t length, int32_t distance);
	void writeBits(uint32_t value, int32_t count);
	void writeHuffman(uint32_t code, int32_t length);
	void flushOutput(bool force);

	Output output;
	std::vector<uint8_t> window; // data from position base on
	int64_t base = 0; // absolute position of window[0]
	int64_t position = 0; // absolute position of the next byte to encode
	std::vector<int64_t> head;
	std::vector<int64_t> prev;
	uint64_t bitBuffer = 0;
	int32_t bitCount = 0;
	std::vector<uint8_t> pending;
	bool finished = false;
};

/**
 * Java: java.util.zip.ZipOutputStream - writes a ZIP archive with deflated entries. Entry names are stored as UTF-8. Archives and entries
 * must stay below 4 GiB (no ZIP64).
 * <pre>
 * ZipWriter zip("archive.zip");
 * zip.addFile("server_console.log", "log/server_console.log");
 * zip.finish();
 * </pre>
 */
class ZipWriter {
public:
	/** Creates (or truncates) the archive. @throws IOException if the file cannot be created */
	explicit ZipWriter(const std::filesystem::path& file);
	~ZipWriter();

	ZipWriter(const ZipWriter&) = delete;
	ZipWriter& operator=(const ZipWriter&) = delete;

	/**
	 * Adds a deflated entry with the contents and modification time of the source file.
	 *
	 * @param entryName name in the archive, with '/' as directory separator
	 * @throws IOException on read or write errors
	 */
	void addFile(std::string_view entryName, const std::filesystem::path& source);

	/** Adds a deflated entry with the given contents and modification time. */
	void addEntry(std::string_view entryName, std::span<const uint8_t> contents, std::chrono::system_clock::time_point modificationTime);

	/** Writes the central directory and closes the file. @throws IOException on write errors */
	void finish();

private:
	struct EntryInfo {
		std::string name;
		uint32_t crc = 0;
		uint64_t compressedSize = 0;
		uint64_t uncompressedSize = 0;
		uint16_t dosTime = 0;
		uint16_t dosDate = 0;
		uint64_t localHeaderOffset = 0;
	};

	/** Writes one entry, whose data is read by the given function into the buffer until it returns 0. */
	void addEntry(std::string_view entryName, std::chrono::system_clock::time_point modificationTime, const std::function<size_t(std::span<uint8_t>)>& read);
	void checkStream(std::string_view action);

	std::filesystem::path file;
	std::ofstream out;
	std::vector<EntryInfo> entries;
	bool finished = false;
};

} // namespace aion::commons::utils
