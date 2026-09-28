#include "aion/gameserver/geoEngine/utils/PngImage.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>

#include "aion/commons/utils/Exception.h"

namespace aion::gameserver::geoEngine::utils {

using commons::utils::IOException;

namespace {

// ---- inflate (RFC 1951) ------------------------------------------------------------------------------------------------------------------------

constexpr int32_t MAX_BITS = 15;

constexpr int32_t FAST_BITS = 9;

/**
 * Canonical Huffman decoding table: symbol counts per code length and the symbols ordered by code, plus a lookup table for codes of at most
 * FAST_BITS bits indexed by the next stream bits (entry: length << 12 | symbol, 0 for longer codes).
 */
struct Huffman {
	std::array<uint16_t, MAX_BITS + 1> counts{};
	std::vector<uint16_t> symbols;
	std::array<uint16_t, 1 << FAST_BITS> fast{};
};

class BitReader {
public:
	explicit BitReader(std::span<const uint8_t> input) : input(input) {}

	uint32_t bits(int32_t count) {
		while (bitCount < count) {
			if (position >= input.size())
				throw IOException("Unexpected end of zlib data");
			buffer |= static_cast<uint32_t>(input[position++]) << bitCount;
			bitCount += 8;
		}
		uint32_t value = buffer & ((1u << count) - 1u);
		buffer >>= count;
		bitCount -= count;
		return value;
	}

	/** Makes up to `count` bits available without consuming them. @return the number of available bits (less only at the end of the input) */
	int32_t fill(int32_t count) {
		while (bitCount < count && position < input.size()) {
			buffer |= static_cast<uint32_t>(input[position++]) << bitCount;
			bitCount += 8;
		}
		return bitCount;
	}

	uint32_t peek(int32_t count) const noexcept { return buffer & ((1u << count) - 1u); }

	void consume(int32_t count) noexcept {
		buffer >>= count;
		bitCount -= count;
	}

	/** Discards the bits up to the next byte boundary (whole bytes read ahead by fill() are given back to the input). */
	void alignToByte() {
		position -= static_cast<size_t>(bitCount / 8);
		buffer = 0;
		bitCount = 0;
	}

	uint8_t byte() {
		if (position >= input.size())
			throw IOException("Unexpected end of zlib data");
		return input[position++];
	}

	size_t bytePosition() const noexcept { return position; }

private:
	std::span<const uint8_t> input;
	size_t position = 0;
	uint32_t buffer = 0;
	int32_t bitCount = 0;
};

void buildHuffman(Huffman& huffman, std::span<const uint8_t> lengths) {
	huffman.counts.fill(0);
	for (uint8_t length : lengths)
		huffman.counts[length]++;
	huffman.counts[0] = 0;
	int32_t left = 1;
	for (int32_t length = 1; length <= MAX_BITS; ++length) {
		left <<= 1;
		left -= huffman.counts[static_cast<size_t>(length)];
		if (left < 0)
			throw IOException("Invalid Huffman code lengths (over-subscribed)");
	}
	std::array<uint16_t, MAX_BITS + 2> offsets{};
	for (int32_t length = 1; length <= MAX_BITS; ++length)
		offsets[static_cast<size_t>(length + 1)] = static_cast<uint16_t>(offsets[static_cast<size_t>(length)] + huffman.counts[static_cast<size_t>(length)]);
	huffman.symbols.assign(lengths.size(), 0);
	for (size_t symbol = 0; symbol < lengths.size(); ++symbol) {
		if (lengths[symbol] != 0)
			huffman.symbols[offsets[lengths[symbol]]++] = static_cast<uint16_t>(symbol);
	}
	// lookup table: canonical codes in (length, symbol) order, bit-reversed because the stream sends a code's most significant bit first
	huffman.fast.fill(0);
	std::array<uint32_t, MAX_BITS + 2> nextCode{};
	uint32_t code = 0;
	for (int32_t length = 1; length <= MAX_BITS; ++length) {
		code = (code + huffman.counts[static_cast<size_t>(length - 1)]) << 1;
		nextCode[static_cast<size_t>(length)] = code;
	}
	for (size_t symbol = 0; symbol < lengths.size(); ++symbol) {
		int32_t length = lengths[symbol];
		if (length == 0)
			continue;
		uint32_t symbolCode = nextCode[static_cast<size_t>(length)]++;
		if (length > FAST_BITS)
			continue;
		uint32_t reversed = 0;
		for (int32_t bit = 0; bit < length; ++bit)
			reversed |= ((symbolCode >> bit) & 1u) << (length - 1 - bit);
		for (uint32_t fillBits = reversed; fillBits < (1u << FAST_BITS); fillBits += 1u << length)
			huffman.fast[fillBits] = static_cast<uint16_t>((length << 12) | static_cast<int32_t>(symbol));
	}
}

int32_t decodeSymbol(BitReader& reader, const Huffman& huffman) {
	if (reader.fill(FAST_BITS) >= FAST_BITS) {
		uint16_t entry = huffman.fast[reader.peek(FAST_BITS)];
		if (entry != 0) {
			reader.consume(entry >> 12);
			return entry & 0x0FFF;
		}
	}
	int32_t code = 0;  // bits read so far, most significant first
	int32_t first = 0; // first code of the current length
	int32_t index = 0; // index of the first symbol of the current length
	for (int32_t length = 1; length <= MAX_BITS; ++length) {
		code |= static_cast<int32_t>(reader.bits(1));
		int32_t count = huffman.counts[static_cast<size_t>(length)];
		if (code - count < first)
			return huffman.symbols[static_cast<size_t>(index + (code - first))];
		index += count;
		first += count;
		first <<= 1;
		code <<= 1;
	}
	throw IOException("Invalid Huffman code");
}

constexpr std::array<uint16_t, 29> LENGTH_BASE{3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
constexpr std::array<uint8_t, 29> LENGTH_EXTRA{0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
constexpr std::array<uint16_t, 30> DISTANCE_BASE{1,   2,   3,   4,   5,   7,    9,    13,   17,   25,   33,   49,   65,    97,    129,
												 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
constexpr std::array<uint8_t, 30> DISTANCE_EXTRA{0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

void inflateCodes(BitReader& reader, std::vector<uint8_t>& out, const Huffman& lengthCodes, const Huffman& distanceCodes) {
	for (;;) {
		int32_t symbol = decodeSymbol(reader, lengthCodes);
		if (symbol < 256) {
			out.push_back(static_cast<uint8_t>(symbol));
		} else if (symbol == 256) {
			return;
		} else {
			symbol -= 257;
			if (symbol >= 29)
				throw IOException("Invalid length symbol");
			size_t length = LENGTH_BASE[static_cast<size_t>(symbol)] + reader.bits(LENGTH_EXTRA[static_cast<size_t>(symbol)]);
			int32_t distanceSymbol = decodeSymbol(reader, distanceCodes);
			if (distanceSymbol >= 30)
				throw IOException("Invalid distance symbol");
			size_t distance = DISTANCE_BASE[static_cast<size_t>(distanceSymbol)] + reader.bits(DISTANCE_EXTRA[static_cast<size_t>(distanceSymbol)]);
			if (distance > out.size())
				throw IOException("Invalid distance (before the start of the data)");
			size_t from = out.size() - distance;
			size_t to = out.size();
			out.resize(to + length);
			uint8_t* bytes = out.data();
			for (size_t i = 0; i < length; ++i)
				bytes[to + i] = bytes[from + i];
		}
	}
}

void inflateStored(BitReader& reader, std::vector<uint8_t>& out) {
	reader.alignToByte();
	uint32_t len = reader.byte();
	len |= static_cast<uint32_t>(reader.byte()) << 8;
	uint32_t nlen = reader.byte();
	nlen |= static_cast<uint32_t>(reader.byte()) << 8;
	if ((len ^ 0xFFFFu) != nlen)
		throw IOException("Invalid stored block length");
	out.reserve(out.size() + len);
	for (uint32_t i = 0; i < len; ++i)
		out.push_back(reader.byte());
}

void inflateFixed(BitReader& reader, std::vector<uint8_t>& out) {
	static const std::pair<Huffman, Huffman> tables = [] {
		std::array<uint8_t, 288> lengths{};
		size_t symbol = 0;
		for (; symbol < 144; ++symbol)
			lengths[symbol] = 8;
		for (; symbol < 256; ++symbol)
			lengths[symbol] = 9;
		for (; symbol < 280; ++symbol)
			lengths[symbol] = 7;
		for (; symbol < 288; ++symbol)
			lengths[symbol] = 8;
		std::pair<Huffman, Huffman> result;
		buildHuffman(result.first, lengths);
		std::array<uint8_t, 30> distanceLengths{};
		distanceLengths.fill(5);
		buildHuffman(result.second, distanceLengths);
		return result;
	}();
	inflateCodes(reader, out, tables.first, tables.second);
}

void inflateDynamic(BitReader& reader, std::vector<uint8_t>& out) {
	int32_t nlen = static_cast<int32_t>(reader.bits(5)) + 257;
	int32_t ndist = static_cast<int32_t>(reader.bits(5)) + 1;
	int32_t ncode = static_cast<int32_t>(reader.bits(4)) + 4;
	if (nlen > 286 || ndist > 30)
		throw IOException("Invalid dynamic block code counts");
	static constexpr std::array<uint8_t, 19> ORDER{16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
	std::array<uint8_t, 19> codeLengthLengths{};
	for (int32_t i = 0; i < ncode; ++i)
		codeLengthLengths[ORDER[static_cast<size_t>(i)]] = static_cast<uint8_t>(reader.bits(3));
	Huffman codeLengthCodes;
	buildHuffman(codeLengthCodes, codeLengthLengths);

	std::vector<uint8_t> lengths(static_cast<size_t>(nlen + ndist), 0);
	for (size_t index = 0; index < lengths.size();) {
		int32_t symbol = decodeSymbol(reader, codeLengthCodes);
		if (symbol < 16) {
			lengths[index++] = static_cast<uint8_t>(symbol);
		} else {
			uint8_t value = 0;
			size_t repeat;
			if (symbol == 16) {
				if (index == 0)
					throw IOException("Repeat of a code length without a previous length");
				value = lengths[index - 1];
				repeat = 3 + reader.bits(2);
			} else if (symbol == 17) {
				repeat = 3 + reader.bits(3);
			} else {
				repeat = 11 + reader.bits(7);
			}
			if (index + repeat > lengths.size())
				throw IOException("Too many code lengths");
			std::fill_n(lengths.begin() + static_cast<std::ptrdiff_t>(index), repeat, value);
			index += repeat;
		}
	}
	if (lengths[256] == 0)
		throw IOException("Missing end-of-block code");
	Huffman lengthCodes;
	Huffman distanceCodes;
	buildHuffman(lengthCodes, std::span<const uint8_t>(lengths).first(static_cast<size_t>(nlen)));
	buildHuffman(distanceCodes, std::span<const uint8_t>(lengths).subspan(static_cast<size_t>(nlen)));
	inflateCodes(reader, out, lengthCodes, distanceCodes);
}

// ---- PNG ---------------------------------------------------------------------------------------------------------------------------------------

uint32_t readUInt32BigEndian(std::span<const uint8_t> data, size_t position) {
	return (static_cast<uint32_t>(data[position]) << 24) | (static_cast<uint32_t>(data[position + 1]) << 16) |
		   (static_cast<uint32_t>(data[position + 2]) << 8) | static_cast<uint32_t>(data[position + 3]);
}

int32_t channelCount(int32_t colorType) {
	switch (colorType) {
		case 0: // grayscale
		case 3: // palette index
			return 1;
		case 2: // RGB
			return 3;
		case 4: // grayscale + alpha
			return 2;
		case 6: // RGBA
			return 4;
		default:
			throw IOException("Invalid PNG color type " + std::to_string(colorType));
	}
}

bool validBitDepth(int32_t colorType, int32_t bitDepth) {
	switch (colorType) {
		case 0:
			return bitDepth == 1 || bitDepth == 2 || bitDepth == 4 || bitDepth == 8 || bitDepth == 16;
		case 3:
			return bitDepth == 1 || bitDepth == 2 || bitDepth == 4 || bitDepth == 8;
		default:
			return bitDepth == 8 || bitDepth == 16;
	}
}

uint8_t paeth(uint8_t a, uint8_t b, uint8_t c) {
	int32_t p = static_cast<int32_t>(a) + b - c;
	int32_t pa = std::abs(p - a);
	int32_t pb = std::abs(p - b);
	int32_t pc = std::abs(p - c);
	if (pa <= pb && pa <= pc)
		return a;
	if (pb <= pc)
		return b;
	return c;
}

/**
 * Reverses the scanline filters of one (sub)image in place: `data` holds `rows` scanlines of `rowBytes` bytes, each preceded by its filter type.
 * @return the unfiltered rows without the filter bytes
 */
std::vector<uint8_t> unfilter(std::span<const uint8_t> data, size_t rows, size_t rowBytes, size_t bytesPerPixel) {
	std::vector<uint8_t> result(rows * rowBytes);
	std::vector<uint8_t> zeroRow(rowBytes, 0);
	for (size_t row = 0; row < rows; ++row) {
		uint8_t filter = data[row * (rowBytes + 1)];
		const uint8_t* in = data.data() + row * (rowBytes + 1) + 1;
		uint8_t* out = result.data() + row * rowBytes;
		const uint8_t* previous = row == 0 ? zeroRow.data() : out - rowBytes;
		const size_t leftBytes = std::min(bytesPerPixel, rowBytes); // bytes without a left neighbour (left and upLeft are 0)
		switch (filter) {
			case 0:
				std::copy_n(in, rowBytes, out);
				break;
			case 1:
				std::copy_n(in, leftBytes, out);
				for (size_t i = leftBytes; i < rowBytes; ++i)
					out[i] = static_cast<uint8_t>(in[i] + out[i - bytesPerPixel]);
				break;
			case 2:
				for (size_t i = 0; i < rowBytes; ++i)
					out[i] = static_cast<uint8_t>(in[i] + previous[i]);
				break;
			case 3:
				for (size_t i = 0; i < leftBytes; ++i)
					out[i] = static_cast<uint8_t>(in[i] + (static_cast<uint32_t>(previous[i]) >> 1));
				for (size_t i = leftBytes; i < rowBytes; ++i)
					out[i] = static_cast<uint8_t>(in[i] + ((static_cast<uint32_t>(out[i - bytesPerPixel]) + previous[i]) >> 1));
				break;
			case 4:
				for (size_t i = 0; i < leftBytes; ++i)
					out[i] = static_cast<uint8_t>(in[i] + paeth(0, previous[i], 0));
				for (size_t i = leftBytes; i < rowBytes; ++i)
					out[i] = static_cast<uint8_t>(in[i] + paeth(out[i - bytesPerPixel], previous[i], previous[i - bytesPerPixel]));
				break;
			default:
				throw IOException("Unknown PNG row filter type " + std::to_string(filter));
		}
	}
	return result;
}

/** Adler-32 of `data` (RFC 1950), summed in blocks of at most 5552 bytes between the modulo reductions like zlib */
uint32_t adler32(std::span<const uint8_t> data) {
	constexpr uint32_t BASE = 65521u;
	constexpr size_t NMAX = 5552; // the largest n with 255 n (n + 1) / 2 + (n + 1) (BASE - 1) < 2^32
	uint32_t a = 1;
	uint32_t b = 0;
	for (size_t offset = 0; offset < data.size();) {
		size_t block = std::min(NMAX, data.size() - offset);
		for (size_t i = 0; i < block; ++i) {
			a += data[offset + i];
			b += a;
		}
		a %= BASE;
		b %= BASE;
		offset += block;
	}
	return (b << 16) | a;
}

/** The inflate of PngImage::inflateZlib, reserving `expectedSize` output bytes (the decoded size PNG images know in advance) */
std::vector<uint8_t> inflateZlibData(std::span<const uint8_t> data, size_t expectedSize) {
	if (data.size() < 2)
		throw IOException("zlib data too short");
	uint8_t cmf = data[0];
	uint8_t flg = data[1];
	if ((cmf & 0x0F) != 8 || (cmf >> 4) > 7 || ((static_cast<uint32_t>(cmf) << 8) | flg) % 31 != 0)
		throw IOException("Invalid zlib header");
	if ((flg & 0x20) != 0)
		throw IOException("zlib preset dictionaries are not supported");
	BitReader reader(data.subspan(2));
	std::vector<uint8_t> out;
	out.reserve(expectedSize);
	bool last;
	do {
		last = reader.bits(1) != 0;
		uint32_t type = reader.bits(2);
		switch (type) {
			case 0:
				inflateStored(reader, out);
				break;
			case 1:
				inflateFixed(reader, out);
				break;
			case 2:
				inflateDynamic(reader, out);
				break;
			default:
				throw IOException("Invalid deflate block type");
		}
	} while (!last);
	reader.alignToByte();
	size_t trailer = 2 + reader.bytePosition();
	if (trailer + 4 > data.size())
		throw IOException("Missing zlib Adler-32 checksum");
	if (adler32(out) != readUInt32BigEndian(data, trailer))
		throw IOException("zlib Adler-32 checksum mismatch");
	return out;
}

} // namespace

std::vector<uint8_t> PngImage::inflateZlib(std::span<const uint8_t> data) {
	return inflateZlibData(data, 0);
}

PngImage PngImage::decode(std::span<const uint8_t> data) {
	static constexpr std::array<uint8_t, 8> SIGNATURE{0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
	if (data.size() < SIGNATURE.size() || !std::equal(SIGNATURE.begin(), SIGNATURE.end(), data.begin()))
		throw IOException("Not a PNG file");

	PngImage image;
	int32_t interlace = 0;
	bool headerSeen = false;
	bool endSeen = false;
	std::vector<uint8_t> compressed;
	std::vector<uint8_t> transparency;
	for (size_t position = SIGNATURE.size(); position + 12 <= data.size();) {
		uint32_t length = readUInt32BigEndian(data, position);
		if (length > data.size() - position - 12)
			throw IOException("PNG chunk exceeds the file");
		std::span<const uint8_t> type = data.subspan(position + 4, 4);
		std::span<const uint8_t> content = data.subspan(position + 8, length);
		position += 12 + length; // length, type, data, CRC
		auto is = [&](const char* name) { return std::equal(type.begin(), type.end(), name); };
		if (is("IHDR")) {
			if (length != 13)
				throw IOException("Invalid IHDR chunk");
			uint32_t width = readUInt32BigEndian(content, 0);
			uint32_t height = readUInt32BigEndian(content, 4);
			if (width == 0 || height == 0 || width > 0x7FFFFFFFu || height > 0x7FFFFFFFu)
				throw IOException("Invalid PNG size");
			image.width = static_cast<int32_t>(width);
			image.height = static_cast<int32_t>(height);
			image.bitDepth = content[8];
			image.colorType = content[9];
			interlace = content[12];
			if (content[10] != 0 || content[11] != 0 || interlace > 1)
				throw IOException("Unsupported PNG compression, filter or interlace method");
			channelCount(image.colorType); // validates the color type
			if (!validBitDepth(image.colorType, image.bitDepth))
				throw IOException("Invalid PNG bit depth " + std::to_string(image.bitDepth));
			headerSeen = true;
		} else if (is("tRNS")) {
			transparency.assign(content.begin(), content.end());
		} else if (is("IDAT")) {
			compressed.insert(compressed.end(), content.begin(), content.end());
		} else if (is("IEND")) {
			endSeen = true;
			break;
		}
	}
	if (!headerSeen || !endSeen || compressed.empty())
		throw IOException("Incomplete PNG file");

	int32_t channels = channelCount(image.colorType);
	size_t samplesPerPixel = static_cast<size_t>(channels);
	bool addAlpha = !transparency.empty() && (image.colorType == 0 || image.colorType == 2) && image.bitDepth >= 8;
	const size_t width = static_cast<size_t>(image.width);
	const size_t height = static_cast<size_t>(image.height);
	const size_t bitsPerPixel = samplesPerPixel * static_cast<size_t>(image.bitDepth);
	// the output size of a non-interlaced image (a hint only, capped so a corrupt header cannot reserve gigabytes)
	const size_t expectedRawSize = interlace == 0 ? std::min<size_t>(height * ((width * bitsPerPixel + 7) / 8 + 1), size_t{1} << 30) : 0;
	std::vector<uint8_t> raw = inflateZlibData(compressed, expectedRawSize);
	const size_t bytesPerPixel = std::max<size_t>(1, bitsPerPixel / 8);

	std::vector<uint8_t> pixels; // rows of the full image without filter bytes
	const size_t rowBytes = (width * bitsPerPixel + 7) / 8;
	if (interlace == 0) {
		if (raw.size() < height * (rowBytes + 1))
			throw IOException("PNG image data too short");
		pixels = unfilter(raw, height, rowBytes, bytesPerPixel);
	} else {
		if (image.bitDepth < 8)
			throw IOException("Interlaced PNG images with fewer than 8 bits per sample are not supported");
		static constexpr std::array<size_t, 7> START_X{0, 4, 0, 2, 0, 1, 0};
		static constexpr std::array<size_t, 7> START_Y{0, 0, 4, 0, 2, 0, 1};
		static constexpr std::array<size_t, 7> STEP_X{8, 8, 4, 4, 2, 2, 1};
		static constexpr std::array<size_t, 7> STEP_Y{8, 8, 8, 4, 4, 2, 2};
		pixels.assign(height * rowBytes, 0);
		size_t offset = 0;
		for (size_t pass = 0; pass < 7; ++pass) {
			size_t passWidth = width > START_X[pass] ? (width - START_X[pass] + STEP_X[pass] - 1) / STEP_X[pass] : 0;
			size_t passHeight = height > START_Y[pass] ? (height - START_Y[pass] + STEP_Y[pass] - 1) / STEP_Y[pass] : 0;
			if (passWidth == 0 || passHeight == 0)
				continue;
			size_t passRowBytes = passWidth * bytesPerPixel;
			if (raw.size() < offset + passHeight * (passRowBytes + 1))
				throw IOException("PNG image data too short");
			std::vector<uint8_t> passPixels =
				unfilter(std::span<const uint8_t>(raw).subspan(offset, passHeight * (passRowBytes + 1)), passHeight, passRowBytes, bytesPerPixel);
			offset += passHeight * (passRowBytes + 1);
			for (size_t y = 0; y < passHeight; ++y) {
				for (size_t x = 0; x < passWidth; ++x) {
					size_t target = ((START_Y[pass] + y * STEP_Y[pass]) * width + START_X[pass] + x * STEP_X[pass]) * bytesPerPixel;
					std::copy_n(passPixels.begin() + static_cast<std::ptrdiff_t>((y * passWidth + x) * bytesPerPixel), bytesPerPixel,
						pixels.begin() + static_cast<std::ptrdiff_t>(target));
				}
			}
		}
	}

	if (image.bitDepth < 8) {
		// Java: MultiPixelPackedSampleModel, the packed rows
		image.dataBufferType = DataBufferType::BYTE;
		image.bytes.assign(pixels.begin(), pixels.end());
		return image;
	}

	const size_t pixelCount = width * height;
	const size_t outputSamples = samplesPerPixel + (addAlpha ? 1 : 0);
	if (image.bitDepth == 8) {
		image.dataBufferType = DataBufferType::BYTE;
		image.bytes.resize(pixelCount * outputSamples);
		for (size_t pixel = 0; pixel < pixelCount; ++pixel) {
			const uint8_t* in = pixels.data() + pixel * samplesPerPixel;
			int8_t* out = image.bytes.data() + pixel * outputSamples;
			bool transparent = addAlpha;
			for (size_t sample = 0; sample < samplesPerPixel; ++sample) {
				out[sample] = static_cast<int8_t>(in[sample]);
				if (addAlpha && (sample * 2 + 1 >= transparency.size() || transparency[sample * 2 + 1] != in[sample]))
					transparent = false;
			}
			if (addAlpha)
				out[samplesPerPixel] = static_cast<int8_t>(transparent ? 0 : 0xFF);
		}
	} else {
		image.dataBufferType = DataBufferType::USHORT;
		image.shorts.resize(pixelCount * outputSamples);
		for (size_t pixel = 0; pixel < pixelCount; ++pixel) {
			const uint8_t* in = pixels.data() + pixel * samplesPerPixel * 2;
			int16_t* out = image.shorts.data() + pixel * outputSamples;
			bool transparent = addAlpha;
			for (size_t sample = 0; sample < samplesPerPixel; ++sample) {
				uint16_t value = static_cast<uint16_t>((in[sample * 2] << 8) | in[sample * 2 + 1]);
				out[sample] = static_cast<int16_t>(value);
				if (addAlpha && (sample * 2 + 1 >= transparency.size() ||
									(static_cast<uint16_t>((transparency[sample * 2] << 8) | transparency[sample * 2 + 1]) != value)))
					transparent = false;
			}
			if (addAlpha)
				out[samplesPerPixel] = static_cast<int16_t>(transparent ? 0 : 0xFFFF);
		}
	}
	return image;
}

PngImage PngImage::read(const std::filesystem::path& file) {
	std::ifstream in(file, std::ios::binary | std::ios::ate);
	if (!in)
		throw IOException("Can't read input file!");
	std::streamoff size = in.tellg();
	std::vector<uint8_t> data(size > 0 ? static_cast<size_t>(size) : 0);
	in.seekg(0);
	if (!data.empty() && !in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size())))
		throw IOException("Can't read input file!");
	return decode(data);
}

} // namespace aion::gameserver::geoEngine::utils
