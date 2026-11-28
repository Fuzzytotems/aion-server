#include "aion/commons/Banner.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "aion/commons/logging/ConsoleAppender.h"
#include "aion/commons/utils/StringUtils.h"

namespace aion::commons {

namespace {

// Java: the text blocks BANNER, BORDER_LEFT and BORDER_RIGHT, one element per line (generated from Banner.java)
// clang-format off
const std::vector<std::string_view> BANNER = {
		"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠠⢤⣤⣤⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
		"⠈⠉⢻⣿⣿⠿⠿⠿⣶⣦⠀⣀⣀⣀⣀⣀⣀⣀⣀⣀⢠⣀⠀⠀⠀⠀⠀⠀⣀⣀⣀⡀⢀⢀⣀⣀⣀⣀⡀⠀⠀⠀⠀⠀⠀⣀⡀⠀⠀⠀⠀⠀⡀⠀⢀⣀⣀⣀⣀⣀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢈⣿⣿⣷⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣠⣾⣇⠀",
		"⠀⠀⢸⣿⣿⠀⠀⠀⢘⣿⡆⠀⢹⣿⣿⠉⠉⠉⠉⢻⠀⢻⣿⡉⠒⠀⠀⣠⣿⠟⢀⢊⣾⡿⠛⠉⠉⠛⢿⣷⣄⠉⠉⣿⣿⣟⠀⠀⠀⠀⢀⣴⣿⡀⠀⣿⣿⡟⠛⠻⢿⣷⣤⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣼⡿⣿⣿⣷⠀⠀⠀⠲⣶⣶⡶⠒⠀⠀⡀⢀⣀⣀⣀⣀⣀⠀⠀⠀⠒⢲⣶⣶⡏⠀⠀⠀⢀⠜⢿⣿⣿⠀",
		"⠀⠀⣸⣿⣿⣤⣤⣤⡾⠟⠀⠀⢸⣿⣿⠀⠀⠀⠀⠀⠀⠀⢝⣿⣆⠀⣰⣿⠋⠀⣼⣿⡟⠀⠀⣴⣦⠀⠀⢻⣿⣇⠀⣿⣿⣿⣆⠀⠀⡠⠊⢻⣿⡇⠀⣿⣿⡇⠀⠀⠀⠘⣿⣿⣆⠀⠀⠀⠀⠀⠀⠀⠀⣰⣿⠃⠘⣿⣿⣧⠀⠀⠀⣿⣿⡇⠀⡠⣡⣾⡿⠛⠉⠉⠙⠻⣿⣶⣄⠀⢸⣿⣿⣧⡀⠀⠀⠀⠀⢸⣿⣿⠀",
		"⠀⠀⢹⣿⣿⠛⠛⠛⠻⣷⣄⠀⢸⣿⣿⣤⣤⣤⣾⠀⠀⠀⠀⠙⢿⣿⡟⠁⠀⠀⣿⣿⡇⠀⢸⣿⣿⡇⠀⢸⣿⣿⠀⣿⣿⡇⠉⠳⣄⠀⠀⢸⣿⡇⠀⣿⣿⡇⠀⠀⠀⠀⢸⣿⣿⠀⠀⠀⠀⠀⠀⢀⣴⣿⣇⣀⣀⣸⣿⣿⣆⠀⠀⣿⣿⡇⢰⣿⣿⠏⠀⠀⣠⣦⡀⠀⠘⣿⣿⣆⢸⣿⣿⠙⠻⣦⡀⠀⠀⢸⣿⣿⠀",
		"⠀⠀⢸⣿⣿⠀⠀⠀⠀⢹⣿⡇⢸⣿⣿⠀⠀⠀⠙⠀⠀⠀⠀⠀⢸⣿⣧⠀⠀⠀⢿⣿⡇⠀⠸⣿⣿⠇⠀⢸⣿⡿⠀⣿⣿⡇⠀⠀⠈⠻⣦⣸⣿⣧⠀⣿⣿⡇⠀⠀⠀⠀⣸⣿⣿⠀⠀⠀⠀⠀⠀⣸⣿⡏⠀⠀⠀⠀⠹⣿⣿⣆⠀⣿⣿⡇⣿⣿⣿⠀⠀⢠⣿⣿⣧⠀⠀⢹⣿⣿⢸⣿⣿⠀⠀⠀⠹⣶⣄⣸⣿⣿⡄",
		"⠀⠀⢸⣿⣿⠀⠀⠀⢀⣾⣿⠃⢸⣿⣿⠀⠀⠀⠀⠀⡀⠀⠀⠀⢸⣿⣿⠀⠀⠀⠘⢻⣿⡄⠀⠈⠁⠀⢠⣿⡿⠃⠀⣿⣿⡇⠀⠀⠀⠀⠈⢻⣿⡟⠀⣿⣿⡇⠀⠀⢀⣴⣿⡿⠃⠀⠀⠀⠀⠀⣰⣿⡟⠀⠀⠀⠀⠀⠀⢻⣿⣿⣆⣿⣿⡇⢹⣿⣿⡀⠀⠈⣿⣿⠏⠀⠀⣾⣿⡿⢸⣿⣿⠀⠀⠀⠀⠀⠙⢿⣿⣿⠃",
		"⠀⠠⠾⠿⠿⠿⠿⠿⠿⠋⠊⠠⠾⠿⠿⠶⠶⠶⠶⠾⠇⠀⠀⠠⠾⠿⠿⠄⠀⠀⠀⠀⠉⠛⠶⠄⠠⠖⠿⠋⠁⠀⠤⠿⠿⠧⠀⠀⠀⠀⠀⠀⠙⠁⠠⠿⠿⠷⠶⠿⠛⠋⠊⠀⠀⠀⠀⠀⠒⠚⠛⠛⠓⠀⠀⠀⠀⠀⠀⠚⠛⠛⣻⣿⣿⣷⡀⠙⢿⣷⡄⠀⠈⠉⠀⢀⣼⣿⡿⣁⣼⣿⣿⣆⠀⠀⠀⠀⠀⠀⠻⡏⠀",
		"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠉⠛⠓⠂⠀⠲⠟⠛⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
};
const std::vector<std::string_view> BORDER_LEFT = {
		" ▟",
		"▚▟",
		"▗",
		"▜",
		"▟",
		"▚",
		"▛",
		"▖▜",
		" ▞",
};
const std::vector<std::string_view> BORDER_RIGHT = {
		" ▚",
		" ▛",
		"  ▙",
		"  ▝",
		" ▛▚",
		"  ▟",
		"  ▛",
		" ▞",
		"▛",
};
// clang-format on

// logback ANSIConstants
constexpr std::string_view ESC_START = "\x1b[";
constexpr std::string_view ESC_END = "m";
constexpr std::string_view BLACK_FG = "30";
constexpr std::string_view RESET = "0;";
constexpr std::string_view DEFAULT_FG = "39";
const std::string BANNER_COLOR = std::string(ESC_START) + "38;5;81;40" + std::string(ESC_END);
constexpr std::string_view BLANK_BRAILLE = "\u2800";

/** Java String.length(): UTF-16 code units */
int32_t length(std::string_view text) {
	return static_cast<int32_t>(utils::StringUtils::utf16Length(text));
}

std::string repeat(std::string_view text, int32_t count) {
	std::string result;
	for (int32_t i = 0; i < count; i++)
		result += text;
	return result;
}

std::string_view trim(std::string_view text) {
	return utils::StringUtils::trim(text);
}

/** C++: the escape sequences are left out when colors is false (the console does not interpret them) */
std::string escape(std::string_view code, bool colors) {
	return colors ? std::string(ESC_START) + std::string(code) + std::string(ESC_END) : std::string();
}

std::string fixBrailleSpacing(std::string_view text, const std::string& color, bool colors) {
	// some monospaced fonts render braille characters wider than others, including the blank one
	std::string replacement = escape(BLACK_FG, colors) + "\u2810" + (colors ? color : std::string());
	return utils::StringUtils::replace(text, BLANK_BRAILLE, replacement);
}

} // namespace

std::string Banner::createBanner(int32_t leftBorderMinWidth, int32_t rightBorderPadding, bool colors) {
	std::string sb;
	for (size_t i = 0; i < BANNER.size(); i++) {
		std::string_view leftBorderLine = BORDER_LEFT[i];
		std::string_view rightBorderLine = trim(BORDER_RIGHT[i]);
		int32_t rightOffset = length(BORDER_RIGHT[i]) - length(rightBorderLine);
		std::string leftPadding = repeat(" ", std::max(0, leftBorderMinWidth - length(leftBorderLine)));
		std::string rightPadding = repeat(BLANK_BRAILLE, std::max(0, rightBorderPadding + rightOffset));
		sb += escape(BLACK_FG, colors);
		sb += leftBorderLine;
		sb += colors ? BANNER_COLOR : std::string();
		sb += leftPadding;
		sb += fixBrailleSpacing(BANNER[i], BANNER_COLOR, colors);
		sb += rightPadding;
		sb += escape(std::string(RESET) + std::string(BLACK_FG), colors);
		sb += rightBorderLine;
		sb += escape(std::string(RESET) + std::string(DEFAULT_FG), colors);
#ifdef _WIN32
		sb += "\r\n"; // System.lineSeparator()
#else
		sb += "\n";
#endif
	}
	return sb;
}

void Banner::print() {
	logging::ConsoleAppender::printToStandardOutput(createBanner(3, 1, logging::ConsoleAppender::standardOutputSupportsColors()));
}

} // namespace aion::commons
