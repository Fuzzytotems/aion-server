// P4-04 PNG decoding for GeoWorldLoader.loadTerrains (Java: ImageIO.read and the raster's DataBuffer). The fixtures (PngFixtures.h) were
// encoded by make_png_fixtures.py with Python's zlib (an independent deflate encoder) and list the samples Java's reader stores: 16-bit
// grayscale as DataBufferUShort, 8-bit grayscale and palette indices as DataBufferByte, packed rows for fewer bits, an added alpha sample
// for tRNS. Error cases: bad signature, truncated data, corrupt checksum.

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "GeoTestSupport.h"
#include "PngFixtures.h"
#include "aion/commons/utils/Exception.h"
#include "aion/gameserver/geoEngine/utils/PngImage.h"

namespace aion::gameserver::geoEngine::test {
namespace {

using utils::PngImage;

TEST(PngImageTest, Grayscale16IsUShortData) {
	PngImage image = PngImage::decode(png::GRAY16_PNG);
	EXPECT_EQ(image.width, png::GRAY16_WIDTH);
	EXPECT_EQ(image.height, png::GRAY16_HEIGHT);
	EXPECT_EQ(image.bitDepth, 16);
	EXPECT_EQ(image.dataBufferType, PngImage::DataBufferType::USHORT);
	EXPECT_EQ(image.shorts, png::GRAY16_SAMPLES);
	EXPECT_TRUE(image.bytes.empty());
	EXPECT_EQ(image.shorts[3], -1) << "0xFFFF, the heightmap's no-data value";
}

TEST(PngImageTest, Grayscale8AndPaletteIndicesAreByteData) {
	PngImage gray = PngImage::decode(png::GRAY8_PNG);
	EXPECT_EQ(gray.dataBufferType, PngImage::DataBufferType::BYTE);
	EXPECT_EQ(gray.width, png::GRAY8_WIDTH);
	EXPECT_EQ(gray.height, png::GRAY8_HEIGHT);
	EXPECT_EQ(gray.bytes, png::GRAY8_SAMPLES);

	PngImage palette = PngImage::decode(png::PALETTE8_PNG);
	EXPECT_EQ(palette.colorType, 3);
	EXPECT_EQ(palette.dataBufferType, PngImage::DataBufferType::BYTE);
	EXPECT_EQ(palette.bytes, png::PALETTE8_SAMPLES) << "the indices, not the palette colors";
}

TEST(PngImageTest, InterlacedStoredBlocks) {
	PngImage image = PngImage::decode(png::ADAM7_PNG);
	EXPECT_EQ(image.width, png::ADAM7_WIDTH);
	EXPECT_EQ(image.bytes, png::ADAM7_SAMPLES);
}

TEST(PngImageTest, SeveralSamplesPerPixelTransparencyAndPackedRows) {
	PngImage rgb = PngImage::decode(png::RGB8_PNG);
	EXPECT_EQ(rgb.bytes, png::RGB8_SAMPLES);
	EXPECT_EQ(rgb.bytes.size(), static_cast<size_t>(png::RGB8_WIDTH * png::RGB8_HEIGHT * 3));

	PngImage transparent = PngImage::decode(png::GRAY16_TRNS_PNG);
	EXPECT_EQ(transparent.dataBufferType, PngImage::DataBufferType::USHORT);
	EXPECT_EQ(transparent.shorts, png::GRAY16_TRNS_SAMPLES);

	PngImage packed = PngImage::decode(png::GRAY4_PNG);
	EXPECT_EQ(packed.bitDepth, 4);
	EXPECT_EQ(packed.dataBufferType, PngImage::DataBufferType::BYTE);
	EXPECT_EQ(packed.bytes, png::GRAY4_SAMPLES);
}

TEST(PngImageTest, InvalidData) {
	std::vector<uint8_t> data = png::GRAY8_PNG;
	data[1] = 'X';
	EXPECT_THROW(PngImage::decode(data), commons::utils::IOException) << "signature";

	std::vector<uint8_t> truncated(png::GRAY8_PNG.begin(), png::GRAY8_PNG.begin() + static_cast<std::ptrdiff_t>(png::GRAY8_PNG.size() / 2));
	EXPECT_THROW(PngImage::decode(truncated), commons::utils::IOException);

	// the Adler-32 checksum is the last 4 bytes of the IDAT data, before the IDAT CRC (4 bytes) and the IEND chunk (12 bytes)
	std::vector<uint8_t> corrupt = png::GRAY16_PNG;
	corrupt[corrupt.size() - 12 - 4 - 1] ^= 0x01;
	EXPECT_THROW(PngImage::decode(corrupt), commons::utils::IOException);

	std::vector<uint8_t> zlib{0x78, 0x9C, 0x03, 0x00, 0x00, 0x00, 0x00, 0x01}; // empty fixed-Huffman block, Adler-32 of nothing is 1
	EXPECT_TRUE(PngImage::inflateZlib(zlib).empty());
	zlib[0] = 0x79;
	EXPECT_THROW(PngImage::inflateZlib(zlib), commons::utils::IOException) << "not deflate";
}

} // namespace
} // namespace aion::gameserver::geoEngine::test
