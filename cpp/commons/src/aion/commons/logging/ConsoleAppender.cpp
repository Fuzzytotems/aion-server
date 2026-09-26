#include "aion/commons/logging/ConsoleAppender.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "aion/commons/logging/PatternLayout.h"
#include "aion/commons/utils/StringUtils.h"

namespace aion::commons::logging {

namespace {

std::FILE* streamOf(ConsoleAppender::Target target) noexcept {
	return target == ConsoleAppender::Target::STDOUT ? stdout : stderr;
}

#ifdef _WIN32
HANDLE handleOf(ConsoleAppender::Target target) noexcept {
	return GetStdHandle(target == ConsoleAppender::Target::STDOUT ? STD_OUTPUT_HANDLE : STD_ERROR_HANDLE);
}

bool isWindowsConsole(ConsoleAppender::Target target) noexcept {
	DWORD mode = 0;
	HANDLE handle = handleOf(target);
	return handle != INVALID_HANDLE_VALUE && handle != nullptr && GetConsoleMode(handle, &mode);
}
#endif

/** @return true if the stream is a terminal that interprets ANSI escape sequences (enabling them on Windows if necessary) */
bool enableColors(ConsoleAppender::Target target) noexcept {
#ifdef _WIN32
	HANDLE handle = handleOf(target);
	DWORD mode = 0;
	if (handle == INVALID_HANDLE_VALUE || handle == nullptr || !GetConsoleMode(handle, &mode))
		return false;
	return (mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) || SetConsoleMode(handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#else
	const char* term = std::getenv("TERM");
	return isatty(fileno(streamOf(target))) && !(term && std::strcmp(term, "dumb") == 0);
#endif
}

} // namespace

ConsoleAppender::ConsoleAppender(std::string_view pattern, const std::chrono::time_zone* timeZone, std::optional<LevelFilter> filter, Target target)
	: ConsoleAppender(pattern, timeZone, filter, target, enableColors(target)) {
}

ConsoleAppender::ConsoleAppender(std::string_view pattern, const std::chrono::time_zone* timeZone, std::optional<LevelFilter> filter, Target target,
	bool colors)
	: base_sink(std::make_unique<PatternLayout>(pattern, timeZone, colors)), target(target), filter(filter), colors(colors) {
#ifdef _WIN32
	windowsConsole = isWindowsConsole(target);
#endif
}

void ConsoleAppender::sink_it_(const spdlog::details::log_msg& msg) {
	if (filter && !filter->accepts(msg.level))
		return;
	buffer.clear();
	formatter_->format(msg, buffer);
	write(std::string_view(buffer.data(), buffer.size()));
}

void ConsoleAppender::write(std::string_view text) {
#ifdef _WIN32
	if (windowsConsole) {
		std::u16string utf16 = utils::StringUtils::toUtf16(text);
		HANDLE handle = handleOf(target);
		const wchar_t* data = reinterpret_cast<const wchar_t*>(utf16.data());
		size_t remaining = utf16.size();
		while (remaining > 0) {
			DWORD written = 0;
			DWORD chunk = static_cast<DWORD>(std::min<size_t>(remaining, 32766));
			if (!WriteConsoleW(handle, data, chunk, &written, nullptr) || written == 0)
				return;
			data += written;
			remaining -= written;
		}
		return;
	}
#endif
	std::FILE* stream = streamOf(target);
	std::fwrite(text.data(), 1, text.size(), stream);
	std::fflush(stream); // Java: immediateFlush
}

void ConsoleAppender::flush_() {
	std::fflush(streamOf(target));
}

} // namespace aion::commons::logging
