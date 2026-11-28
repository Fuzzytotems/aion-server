#pragma once

#include <cstdint>
#include <string>

namespace aion::commons {

/**
 * Java: com.aionemu.commons.Banner - the "Beyond Aion" banner that Logging::init prints on the console.
 * <p>
 * C++: the color escape sequences are only written to a console that interprets them (like ConsoleAppender), and the text goes through
 * ConsoleAppender's output, so the braille characters show on a Windows console whatever its code page.
 */
class Banner {
public:
	static void print();

	/** Java private; C++ public for the tests. @param colors false leaves the escape sequences out */
	static std::string createBanner(int32_t leftBorderMinWidth, int32_t rightBorderPadding, bool colors);

private:
	Banner() = delete;
};

} // namespace aion::commons
