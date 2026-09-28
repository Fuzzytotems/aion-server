#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

#include "aion/gameserver/geoEngine/utils/fwd.h"

namespace aion::gameserver::geoEngine::utils {

/**
 * C++ only: the PNG decoding GeoWorldLoader needs from javax.imageio.ImageIO.read: the raster data buffer of the BufferedImage that Java's PNG
 * reader creates (handlers-and-porting-plan.md §2.6 P4-04). A self-written decoder (zlib inflate, scanline filters, Adam7), no library.
 * <p>
 * Java's reader stores the raw samples: an 8-bit PNG (or one with fewer bits, packed) gets a DataBufferByte, a 16-bit PNG a DataBufferUShort.
 * Grayscale samples, palette indices (not the palette colors) and heightmap values are therefore taken as they are in the file. Images with
 * several samples per pixel keep the PNG sample order here (Deviation: Java reorders RGB to BGR and RGBA to ABGR; GeoWorldLoader only accepts
 * one sample per pixel, so the order is never read). A tRNS chunk of a grayscale or RGB image adds an alpha sample like Java's reader (JDK 11
 * and later). Deviation: interlaced images with fewer than 8 bits per sample are rejected (IOException), which Java decodes; chunk CRCs are
 * not checked. No data file is interlaced.
 * <p>
 * Thread-safety: stateless functions.
 */
class PngImage {
public:
	/** Java: the DataBuffer type of the decoded BufferedImage's raster */
	enum class DataBufferType : uint8_t { BYTE, USHORT };

	int32_t width = 0;
	int32_t height = 0;
	int32_t bitDepth = 0;
	int32_t colorType = 0;
	DataBufferType dataBufferType = DataBufferType::BYTE;
	/** DataBufferByte.getData(): samples of 8-bit images, packed rows of images with fewer bits */
	std::vector<int8_t> bytes;
	/** DataBufferUShort.getData(): samples of 16-bit images */
	std::vector<int16_t> shorts;

	/**
	 * Decodes a PNG file.
	 *
	 * @throws commons::utils::IOException for a file that cannot be read or is not a valid PNG
	 */
	static PngImage read(const std::filesystem::path& file);

	/**
	 * Decodes PNG data.
	 *
	 * @throws commons::utils::IOException for data that is not a valid PNG
	 */
	static PngImage decode(std::span<const uint8_t> data);

	/**
	 * zlib (RFC 1950) decompression with inflate (RFC 1951), including the Adler-32 check.
	 *
	 * @throws commons::utils::IOException for invalid data
	 */
	static std::vector<uint8_t> inflateZlib(std::span<const uint8_t> data);
};

} // namespace aion::gameserver::geoEngine::utils
