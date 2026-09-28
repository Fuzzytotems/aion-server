#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace aion::gameserver::utils {

/**
 * Java: java.awt.Color in the subset the game server uses: an sRGB color with alpha, the named constants and getRed/getGreen/getBlue/getRGB
 * (handlers-and-porting-plan.md §1.10). ChatUtil.color takes it, and the Dye command's reflective lookup
 * `Class.forName("java.awt.Color").getField(NAME)` becomes byName(NAME).
 * <p>
 * An immutable value type (K5); thread-safe.
 */
class JavaColor {
public:
	/**
	 * Java: Color(int r, int g, int b) - an opaque color (alpha 255)
	 *
	 * @throws IllegalArgumentException
	 *           if a component is outside of 0-255
	 */
	JavaColor(int32_t r, int32_t g, int32_t b);

	/**
	 * Java: Color(int r, int g, int b, int a)
	 *
	 * @throws IllegalArgumentException
	 *           if a component is outside of 0-255
	 */
	JavaColor(int32_t r, int32_t g, int32_t b, int32_t a);

	/** Java: Color(int rgb) - an opaque color from the red (bits 16-23), green (8-15) and blue (0-7) components; the other bits are ignored */
	constexpr explicit JavaColor(int32_t rgb) noexcept : value(static_cast<int32_t>(0xff000000u | static_cast<uint32_t>(rgb))) {}

	/** Java: Color.getRed() */
	constexpr int32_t getRed() const noexcept { return (value >> 16) & 0xFF; }

	/** Java: Color.getGreen() */
	constexpr int32_t getGreen() const noexcept { return (value >> 8) & 0xFF; }

	/** Java: Color.getBlue() */
	constexpr int32_t getBlue() const noexcept { return value & 0xFF; }

	/** Java: Color.getAlpha() */
	constexpr int32_t getAlpha() const noexcept { return (value >> 24) & 0xFF; }

	/** Java: Color.getRGB() - the ARGB value, e.g. 0xFFFF0000 (-65536) for RED */
	constexpr int32_t getRGB() const noexcept { return value; }

	/** Java: Color.equals(Object) - same ARGB value */
	constexpr bool equals(const JavaColor& other) const noexcept { return value == other.value; }

	/** Java: Color.hashCode() - the ARGB value */
	constexpr int32_t hashCode() const noexcept { return value; }

	friend constexpr bool operator==(const JavaColor& a, const JavaColor& b) noexcept { return a.value == b.value; }

	/**
	 * Java: the public static final upper-case constants of java.awt.Color by field name (WHITE, LIGHT_GRAY, GRAY, DARK_GRAY, BLACK, RED, PINK,
	 * ORANGE, YELLOW, GREEN, MAGENTA, CYAN, BLUE), as the Dye command reads them with `getField(name)`.
	 *
	 * @return the color, std::nullopt where Java's getField throws NoSuchFieldException (any other name, including the lower-case aliases)
	 */
	static std::optional<JavaColor> byName(std::string_view fieldName) noexcept;

	static const JavaColor WHITE;
	static const JavaColor LIGHT_GRAY;
	static const JavaColor GRAY;
	static const JavaColor DARK_GRAY;
	static const JavaColor BLACK;
	static const JavaColor RED;
	static const JavaColor PINK;
	static const JavaColor ORANGE;
	static const JavaColor YELLOW;
	static const JavaColor GREEN;
	static const JavaColor MAGENTA;
	static const JavaColor CYAN;
	static const JavaColor BLUE;

private:
	int32_t value;
};

inline constinit const JavaColor JavaColor::WHITE{0xFFFFFF};
inline constinit const JavaColor JavaColor::LIGHT_GRAY{0xC0C0C0};
inline constinit const JavaColor JavaColor::GRAY{0x808080};
inline constinit const JavaColor JavaColor::DARK_GRAY{0x404040};
inline constinit const JavaColor JavaColor::BLACK{0x000000};
inline constinit const JavaColor JavaColor::RED{0xFF0000};
inline constinit const JavaColor JavaColor::PINK{0xFFAFAF};
inline constinit const JavaColor JavaColor::ORANGE{0xFFC800};
inline constinit const JavaColor JavaColor::YELLOW{0xFFFF00};
inline constinit const JavaColor JavaColor::GREEN{0x00FF00};
inline constinit const JavaColor JavaColor::MAGENTA{0xFF00FF};
inline constinit const JavaColor JavaColor::CYAN{0x00FFFF};
inline constinit const JavaColor JavaColor::BLUE{0x0000FF};

} // namespace aion::gameserver::utils
