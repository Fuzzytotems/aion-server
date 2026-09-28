#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "aion/commons/utils/ByteBuffer.h"
#include "aion/gameserver/utils/captcha/fwd.h"

namespace aion::gameserver::utils::captcha {

/**
 * C++: a static-only class (fieldmap K5). Java converts a java.awt.image.BufferedImage; the port takes the image as Image, its pixels in the
 * ARGB (not premultiplied) form that BufferedImage.getRGB returns, row by row. The protected Java helpers keep their names; Java's Color[] and
 * int[] arrays are std::vector and std::array.
 *
 * @author Cura
 */
class DDSConverter {
private:
	static constexpr int32_t DDSD_CAPS = 0x0001;
	static constexpr int32_t DDSD_HEIGHT = 0x0002;
	static constexpr int32_t DDSD_WIDTH = 0x0004;
	static constexpr int32_t DDSD_PIXELFORMAT = 0x1000;
	static constexpr int32_t DDSD_MIPMAPCOUNT = 0x20000;
	static constexpr int32_t DDSD_LINEARSIZE = 0x80000;
	static constexpr int32_t DDPF_FOURCC = 0x0004;
	static constexpr int32_t DDSCAPS_TEXTURE = 0x1000;

public:
	DDSConverter() = delete;

	/** C++ only: the parts of java.awt.image.BufferedImage the converter reads */
	struct Image {
		int32_t width = 0;
		int32_t height = 0;
		/** width * height ARGB pixels, row by row */
		std::vector<int32_t> argb;

		int32_t getWidth() const noexcept { return width; }
		int32_t getHeight() const noexcept { return height; }
		/** Java: BufferedImage.getRGB(x, y) */
		int32_t getRGB(int32_t x, int32_t y) const { return argb.at(static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)); }
	};

	/** Java: protected static class Color (members public, hub-headers.md §9.3) */
	class Color {
	public:
		int32_t r, g, b;

		Color() : r(0), g(0), b(0) {}

		Color(int32_t rValue, int32_t gValue, int32_t bValue) : r(rValue), g(gValue), b(bValue) {}

		bool equals(const Color& color) const { return b == color.b && g == color.g && r == color.r; }

		int32_t hashCode() const {
			uint32_t result = static_cast<uint32_t>(r);
			result = 29 * result + static_cast<uint32_t>(g);
			result = 29 * result + static_cast<uint32_t>(b);
			return static_cast<int32_t>(result);
		}
	};

	/** @return the DXT1 texture (a 128 byte header plus 8 bytes per 4x4 tile, little endian), std::nullopt for a null image */
	static std::optional<commons::utils::ByteBuffer> convertToDxt1NoTransparency(const Image* image);

protected:
	static void buildHeaderDxt1(commons::utils::ByteBuffer& buffer, int32_t width, int32_t height);

	static std::array<int32_t, 2> determineExtremeColors(std::span<const Color> colors);

	static int64_t computeBitMask(std::span<const Color> colors, std::array<int32_t, 2> extremaIndices);

	static int32_t getPixel565(const Color& color);

	static Color getColor565(int32_t pixel);

	static std::vector<Color> getColors888(std::span<const int32_t> pixels);

	static int32_t distance(const Color& ca, const Color& cb);
};

} // namespace aion::gameserver::utils::captcha
