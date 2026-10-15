#include "aion/gameserver/utils/captcha/DDSConverter.h"

#include <limits>

namespace aion::gameserver::utils::captcha {

std::optional<commons::utils::ByteBuffer> DDSConverter::convertToDxt1NoTransparency(const Image* image) {
	if (image == nullptr)
		return std::nullopt;
	std::array<int32_t, 16> pixels{};
	int32_t bufferSize = 128 + image->getWidth() * image->getHeight() / 2;
	commons::utils::ByteBuffer buffer = commons::utils::ByteBuffer::allocate(bufferSize);
	buffer.order(commons::utils::ByteOrder::LITTLE_ENDIAN_ORDER);
	buildHeaderDxt1(buffer, image->getWidth(), image->getHeight());
	int32_t numTilesWide = image->getWidth() / 4;
	int32_t numTilesHigh = image->getHeight() / 4;
	for (int32_t i = 0; i < numTilesHigh; i++) {
		for (int32_t j = 0; j < numTilesWide; j++) {
			// Java: image.getSubimage(j * 4, i * 4, 4, 4).getRGB(0, 0, 4, 4, pixels, 0, 4)
			for (int32_t y = 0; y < 4; y++) {
				for (int32_t x = 0; x < 4; x++)
					pixels[static_cast<size_t>(y * 4 + x)] = image->getRGB(j * 4 + x, i * 4 + y);
			}
			std::vector<Color> colors = getColors888(pixels);
			for (size_t k = 0; k < pixels.size(); k++) {
				pixels[k] = getPixel565(colors[k]);
				colors[k] = getColor565(pixels[k]);
			}
			std::array<int32_t, 2> extremaIndices = determineExtremeColors(colors);
			if (pixels[static_cast<size_t>(extremaIndices[0])] < pixels[static_cast<size_t>(extremaIndices[1])]) {
				int32_t t = extremaIndices[0];
				extremaIndices[0] = extremaIndices[1];
				extremaIndices[1] = t;
			}
			buffer.putShort(static_cast<int16_t>(pixels[static_cast<size_t>(extremaIndices[0])]));
			buffer.putShort(static_cast<int16_t>(pixels[static_cast<size_t>(extremaIndices[1])]));
			int64_t bitmask = computeBitMask(colors, extremaIndices);
			buffer.putInt(static_cast<int32_t>(bitmask));
		}
	}
	return buffer;
}

void DDSConverter::buildHeaderDxt1(commons::utils::ByteBuffer& buffer, int32_t width, int32_t height) {
	buffer.rewind();
	buffer.put(static_cast<uint8_t>('D'));
	buffer.put(static_cast<uint8_t>('D'));
	buffer.put(static_cast<uint8_t>('S'));
	buffer.put(static_cast<uint8_t>(' '));
	buffer.putInt(124);
	int32_t flag = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT | DDSD_MIPMAPCOUNT | DDSD_LINEARSIZE;
	buffer.putInt(flag);
	buffer.putInt(height);
	buffer.putInt(width);
	buffer.putInt(width * height / 2);
	buffer.putInt(0); // depth
	buffer.putInt(0); // mipmap count
	buffer.position(buffer.position() + 44); // 11 unused double-words
	buffer.putInt(32); // pixel format size
	buffer.putInt(DDPF_FOURCC);
	buffer.put(static_cast<uint8_t>('D'));
	buffer.put(static_cast<uint8_t>('X'));
	buffer.put(static_cast<uint8_t>('T'));
	buffer.put(static_cast<uint8_t>('1'));
	buffer.putInt(0); // bits per pixel for RGB (non-compressed) formats
	buffer.putInt(0); // rgb bit masks for RGB formats
	buffer.putInt(0); // rgb bit masks for RGB formats
	buffer.putInt(0); // rgb bit masks for RGB formats
	buffer.putInt(0); // alpha mask for RGB formats
	buffer.putInt(DDSCAPS_TEXTURE);
	buffer.putInt(0); // ddsCaps2
	buffer.position(buffer.position() + 12); // 3 unused double-words
}

std::array<int32_t, 2> DDSConverter::determineExtremeColors(std::span<const Color> colors) {
	int32_t farthest = std::numeric_limits<int32_t>::min();
	std::array<int32_t, 2> ex{};
	for (size_t i = 0; i + 1 < colors.size(); i++) {
		for (size_t j = i + 1; j < colors.size(); j++) {
			int32_t d = distance(colors[i], colors[j]);
			if (d > farthest) {
				farthest = d;
				ex[0] = static_cast<int32_t>(i);
				ex[1] = static_cast<int32_t>(j);
			}
		}
	}
	return ex;
}

int64_t DDSConverter::computeBitMask(std::span<const Color> colors, std::array<int32_t, 2> extremaIndices) {
	std::array<Color, 4> colorPoints{};
	colorPoints[0] = colors[static_cast<size_t>(extremaIndices[0])];
	colorPoints[1] = colors[static_cast<size_t>(extremaIndices[1])];
	if (colorPoints[0].equals(colorPoints[1]))
		return 0;
	colorPoints[2].r = (2 * colorPoints[0].r + colorPoints[1].r + 1) / 3;
	colorPoints[2].g = (2 * colorPoints[0].g + colorPoints[1].g + 1) / 3;
	colorPoints[2].b = (2 * colorPoints[0].b + colorPoints[1].b + 1) / 3;
	colorPoints[3].r = (colorPoints[0].r + 2 * colorPoints[1].r + 1) / 3;
	colorPoints[3].g = (colorPoints[0].g + 2 * colorPoints[1].g + 1) / 3;
	colorPoints[3].b = (colorPoints[0].b + 2 * colorPoints[1].b + 1) / 3;
	int64_t bitmask = 0;
	for (size_t i = 0; i < colors.size(); i++) {
		int32_t closest = std::numeric_limits<int32_t>::max();
		int32_t mask = 0;
		for (size_t j = 0; j < colorPoints.size(); j++) {
			int32_t d = distance(colors[i], colorPoints[j]);
			if (d < closest) {
				closest = d;
				mask = static_cast<int32_t>(j);
			}
		}
		// Java: bitmask |= mask << i * 2 - an int shift (i < 16, so the count is below 32) sign-extended to long
		bitmask |= static_cast<int32_t>(static_cast<uint32_t>(mask) << (i * 2));
	}
	return bitmask;
}

int32_t DDSConverter::getPixel565(const Color& color) {
	int32_t r = color.r >> 3;
	int32_t g = color.g >> 2;
	int32_t b = color.b >> 3;
	return r << 11 | g << 5 | b;
}

DDSConverter::Color DDSConverter::getColor565(int32_t pixel) {
	Color color;
	color.r = static_cast<int32_t>((static_cast<int64_t>(pixel) & 0xf800) >> 11);
	color.g = static_cast<int32_t>((static_cast<int64_t>(pixel) & 0x07e0) >> 5);
	color.b = static_cast<int32_t>(static_cast<int64_t>(pixel) & 0x001f);
	return color;
}

std::vector<DDSConverter::Color> DDSConverter::getColors888(std::span<const int32_t> pixels) {
	std::vector<Color> colors(pixels.size());
	for (size_t i = 0; i < pixels.size(); i++) {
		colors[i].r = static_cast<int32_t>((static_cast<int64_t>(pixels[i]) & 0xff0000) >> 16);
		colors[i].g = static_cast<int32_t>((static_cast<int64_t>(pixels[i]) & 0x00ff00) >> 8);
		colors[i].b = static_cast<int32_t>(static_cast<int64_t>(pixels[i]) & 0x0000ff);
	}
	return colors;
}

int32_t DDSConverter::distance(const Color& ca, const Color& cb) {
	return (cb.r - ca.r) * (cb.r - ca.r) + (cb.g - ca.g) * (cb.g - ca.g) + (cb.b - ca.b) * (cb.b - ca.b);
}

} // namespace aion::gameserver::utils::captcha
