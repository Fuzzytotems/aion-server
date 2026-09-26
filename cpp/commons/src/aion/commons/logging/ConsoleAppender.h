#pragma once

#include <chrono>
#include <mutex>
#include <optional>
#include <string_view>

#include <spdlog/sinks/base_sink.h>

#include "aion/commons/logging/LevelFilter.h"

namespace aion::commons::logging {

/**
 * Java: ch.qos.logback.core.ConsoleAppender with a PatternLayoutEncoder - writes formatted messages to the standard output (or error)
 * stream and flushes after each message.
 * <p>
 * Colors (the ANSI escape sequences of %highlight etc.) are only written to terminals that support them: on Windows, virtual terminal
 * processing is enabled for the console if needed, and text is written as UTF-16 so it displays correctly regardless of the console code page.
 * Redirected output gets plain UTF-8 text without escape sequences.
 */
class ConsoleAppender final : public spdlog::sinks::base_sink<std::mutex> {
public:
	enum class Target { STDOUT, STDERR };

	/**
	 * @param pattern logback conversion pattern, see PatternLayout
	 * @param timeZone time zone of dates, nullptr for the system default time zone
	 * @param filter optional filter (logback: filter element)
	 * @param target the stream to write to (logback: target, default System.out)
	 */
	explicit ConsoleAppender(std::string_view pattern, const std::chrono::time_zone* timeZone = nullptr, std::optional<LevelFilter> filter = std::nullopt,
		Target target = Target::STDOUT);

	/** @return true if color escape sequences are written */
	bool isColorEnabled() const noexcept { return colors; }

protected:
	void sink_it_(const spdlog::details::log_msg& msg) override;
	void flush_() override;

private:
	ConsoleAppender(std::string_view pattern, const std::chrono::time_zone* timeZone, std::optional<LevelFilter> filter, Target target, bool colors);

	void write(std::string_view text);

	Target target;
	std::optional<LevelFilter> filter;
	bool colors;
	bool windowsConsole = false;
	spdlog::memory_buf_t buffer;
};

} // namespace aion::commons::logging
