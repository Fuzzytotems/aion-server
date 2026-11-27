#include "aion/gameserver/utils/xml/CompressUtil.h"

#include <array>
#include <limits>
#include <new>
#include <string>

#include <zlib.h>

#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::utils::xml {

namespace {

/** Java's 1024-byte buffer: each inflate and deflate call gets at most this much output space, as in CompressUtil.java */
constexpr uInt BUFFER_SIZE = 1024;

/** a Java byte[] holds at most Integer.MAX_VALUE bytes */
uInt inputLength(std::span<const uint8_t> bytes) {
	if (bytes.size() > static_cast<size_t>(std::numeric_limits<int32_t>::max()))
		throw runtime::IllegalArgumentException("input too large: " + std::to_string(bytes.size()));
	return static_cast<uInt>(bytes.size());
}

[[noreturn]] void zlibFailure(int result, const z_stream& stream, const char* what) {
	if (result == Z_MEM_ERROR)
		throw std::bad_alloc(); // Java: OutOfMemoryError
	throw runtime::IllegalStateException(stream.msg ? stream.msg : what); // Java: InternalError
}

/** java.util.zip.Inflater(): inflateInit (zlib wrapper, windowBits 15); the destructor is Inflater.close() (inflateEnd) */
class Inflater {
public:
	explicit Inflater(std::span<const uint8_t> input) {
		stream.next_in = const_cast<Bytef*>(input.data()); // zlib never writes through next_in
		stream.avail_in = inputLength(input);
		if (int result = inflateInit(&stream); result != Z_OK)
			zlibFailure(result, stream, "inflateInit failed");
	}

	~Inflater() { inflateEnd(&stream); }

	Inflater(const Inflater&) = delete;
	Inflater& operator=(const Inflater&) = delete;

	/** Java Inflater.inflate(byte[]): zlib's inflate with Z_PARTIAL_FLUSH, @return the number of bytes written */
	uInt inflate(Bytef* buffer, uInt length) {
		stream.next_out = buffer;
		stream.avail_out = length;
		int result = ::inflate(&stream, Z_PARTIAL_FLUSH);
		switch (result) {
			case Z_STREAM_END:
				finished_ = true; // bytes after the end of the stream stay unread (Inflater.getRemaining)
				break;
			case Z_OK:
			case Z_NEED_DICT: // Java: needsDictionary() becomes true and nothing more is written
			case Z_BUF_ERROR: // Java: no progress (input used up), 0 bytes written
				break;
			case Z_DATA_ERROR:
				// Java: java.util.zip.DataFormatException with zlib's message
				throw runtime::IllegalArgumentException(stream.msg ? stream.msg : "invalid data");
			default:
				zlibFailure(result, stream, "inflate failed");
		}
		return length - stream.avail_out;
	}

	bool finished() const { return finished_; }

private:
	z_stream stream{};
	bool finished_ = false;
};

/** java.util.zip.Deflater(): level 6 (DEFAULT_COMPRESSION), zlib wrapper, windowBits 15, memLevel 8 (DEF_MEM_LEVEL), default strategy */
class Deflater {
public:
	explicit Deflater(std::span<const uint8_t> input) {
		stream.next_in = const_cast<Bytef*>(input.data()); // zlib never writes through next_in
		stream.avail_in = inputLength(input);
		if (int result = deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, MAX_WBITS, 8, Z_DEFAULT_STRATEGY); result != Z_OK)
			zlibFailure(result, stream, "deflateInit2 failed");
	}

	~Deflater() { deflateEnd(&stream); }

	Deflater(const Deflater&) = delete;
	Deflater& operator=(const Deflater&) = delete;

	/** Java Deflater.deflate(byte[]) after finish(): zlib's deflate with Z_FINISH, @return the number of bytes written */
	uInt deflate(Bytef* buffer, uInt length) {
		stream.next_out = buffer;
		stream.avail_out = length;
		int result = ::deflate(&stream, Z_FINISH);
		if (result == Z_STREAM_END)
			finished_ = true;
		else if (result != Z_OK && result != Z_BUF_ERROR)
			zlibFailure(result, stream, "deflate failed");
		return length - stream.avail_out;
	}

	bool finished() const { return finished_; }

private:
	z_stream stream{};
	bool finished_ = false;
};

} // namespace

std::vector<uint8_t> CompressUtil::decompress(std::span<const uint8_t> bytes) {
	// Create an expandable byte array to hold the decompressed data
	std::vector<uint8_t> bos;
	bos.reserve(bytes.size());

	std::array<Bytef, BUFFER_SIZE> buffer;
	Inflater decompressor(bytes);
	while (true) {
		uInt count = decompressor.inflate(buffer.data(), BUFFER_SIZE);
		if (count > 0)
			bos.insert(bos.end(), buffer.begin(), buffer.begin() + count);
		else if (decompressor.finished())
			break;
		else
			throw runtime::IllegalStateException("Bad zip data, size: " + std::to_string(bytes.size()));
	}

	return bos;
}

std::vector<uint8_t> CompressUtil::compress(std::span<const uint8_t> bytes) {
	// Create an expandable byte array to hold the compressed data
	std::vector<uint8_t> bos;

	std::array<Bytef, BUFFER_SIZE> buffer;
	Deflater compressor(bytes);
	while (!compressor.finished()) {
		uInt count = compressor.deflate(buffer.data(), BUFFER_SIZE);
		bos.insert(bos.end(), buffer.begin(), buffer.begin() + count);
	}

	return bos;
}

} // namespace aion::gameserver::utils::xml
