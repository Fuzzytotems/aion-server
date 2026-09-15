#include "aion/gameserver/utils/JavaColor.h"

#include <array>
#include <string>
#include <utility>

#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::utils {

namespace {

/** Java: Color.testColorValueRange (the bad components in the order Alpha, Red, Green, Blue) */
void testColorValueRange(int32_t r, int32_t g, int32_t b, int32_t a) {
	bool rangeError = false;
	std::string badComponentString;
	if (a < 0 || a > 255) {
		rangeError = true;
		badComponentString += " Alpha";
	}
	if (r < 0 || r > 255) {
		rangeError = true;
		badComponentString += " Red";
	}
	if (g < 0 || g > 255) {
		rangeError = true;
		badComponentString += " Green";
	}
	if (b < 0 || b > 255) {
		rangeError = true;
		badComponentString += " Blue";
	}
	if (rangeError)
		throw runtime::IllegalArgumentException("Color parameter outside of expected range:" + badComponentString);
}

int32_t argb(int32_t r, int32_t g, int32_t b, int32_t a) {
	testColorValueRange(r, g, b, a);
	return static_cast<int32_t>((static_cast<uint32_t>(a & 0xFF) << 24) | (static_cast<uint32_t>(r & 0xFF) << 16) |
		(static_cast<uint32_t>(g & 0xFF) << 8) | static_cast<uint32_t>(b & 0xFF));
}

} // namespace

JavaColor::JavaColor(int32_t r, int32_t g, int32_t b) : JavaColor(r, g, b, 255) {
}

JavaColor::JavaColor(int32_t r, int32_t g, int32_t b, int32_t a) : value(argb(r, g, b, a)) {
}

std::optional<JavaColor> JavaColor::byName(std::string_view fieldName) noexcept {
	static constexpr std::array<std::pair<std::string_view, const JavaColor*>, 13> colors{{
		{"WHITE", &WHITE},
		{"LIGHT_GRAY", &LIGHT_GRAY},
		{"GRAY", &GRAY},
		{"DARK_GRAY", &DARK_GRAY},
		{"BLACK", &BLACK},
		{"RED", &RED},
		{"PINK", &PINK},
		{"ORANGE", &ORANGE},
		{"YELLOW", &YELLOW},
		{"GREEN", &GREEN},
		{"MAGENTA", &MAGENTA},
		{"CYAN", &CYAN},
		{"BLUE", &BLUE},
	}};
	for (const auto& [name, color] : colors) {
		if (name == fieldName)
			return *color;
	}
	return std::nullopt;
}

} // namespace aion::gameserver::utils
