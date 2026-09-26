#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include <spdlog/formatter.h>
#include <spdlog/pattern_formatter.h>

namespace aion::commons::logging {

/**
 * Java: ch.qos.logback.classic.PatternLayout (as used by PatternLayoutEncoder) - formats log messages with a logback conversion pattern, so the
 * patterns of the servers' logback.xml files can be used unchanged (after substituting ${...} properties).
 * <p>
 * Supported conversion words (with logback's format modifiers like <tt>%-5level</tt>, <tt>%.30logger</tt>, <tt>%-10.-20thread</tt>):
 * <table>
 * <tr><td>%d / %date{pattern[, zoneId]}</td><td>date (java.time pattern, default "yyyy-MM-dd HH:mm:ss,SSS"); the zone defaults to the
 * layout's time zone</td></tr>
 * <tr><td>%level / %le / %p</td><td>TRACE, DEBUG, INFO, WARN or ERROR (spdlog's critical is ERROR)</td></tr>
 * <tr><td>%thread / %t</td><td>name of the logging thread (utils::concurrent::getCurrentThreadName)</td></tr>
 * <tr><td>%logger / %lo / %c{length}</td><td>logger name, {0} for the part after the last dot, other lengths abbreviate like logback</td></tr>
 * <tr><td>%message / %msg / %m</td><td>the message</td></tr>
 * <tr><td>%n</td><td>line separator of the platform (like Java's System.lineSeparator())</td></tr>
 * <tr><td>%ex / %exception / %throwable, %nopex / %nopexception</td><td>see below</td></tr>
 * <tr><td>%highlight(...), %black(...), %red(...), %green(...), %yellow(...), %blue(...), %magenta(...), %cyan(...), %white(...), %gray(...),
 * %boldRed(...) and the other bold colors</td><td>ANSI colors like logback (plain output if colors are disabled)</td></tr>
 * <tr><td>%replace(...){regex, replacement}</td><td>regular expression replacement (ECMAScript syntax; Java's $1 group references work)</td></tr>
 * <tr><td>%(...)</td><td>grouping, e.g. for a common format modifier</td></tr>
 * </table>
 * Escapes: <tt>\%</tt>, <tt>\(</tt>, <tt>\)</tt>, <tt>\{</tt>, <tt>\}</tt>, <tt>\\</tt>, <tt>\t</tt>, <tt>\n</tt>, <tt>\r</tt>, <tt>\_</tt> (nothing).
 * <p>
 * Exceptions: Logger passes the exception text along with the message (see detail::getThrowableStart), so %message prints the message only and
 * %ex the exception, each of its lines followed by the platform line separator. Like logback, the exception is appended after the formatted
 * message if the pattern has no throwable conversion word (%ex, %exception, %throwable, %nopex, %nopexception). Messages logged without an
 * exception, or not through Logger, are printed by %message as they are.
 * <p>
 * An unknown conversion word is printed as "%PARSER_ERROR[word]" and reported on stderr, like logback does.
 * <p>
 * Like spdlog's formatters, a layout is not thread safe: sinks call it under their lock.
 */
class PatternLayout final : public spdlog::formatter {
public:
	/**
	 * @param pattern the conversion pattern
	 * @param timeZone the time zone of %date without zone option, nullptr for the system default time zone
	 * @param enableColors if false, color conversion words output their content without escape sequences
	 * @throws IllegalArgumentException if the pattern is invalid (e.g. unbalanced parentheses or an invalid %replace regex)
	 */
	explicit PatternLayout(std::string_view pattern, const std::chrono::time_zone* timeZone = nullptr, bool enableColors = true);
	~PatternLayout() override;

	void format(const spdlog::details::log_msg& msg, spdlog::memory_buf_t& dest) override;

	/** @return the formatted message */
	std::string format(const spdlog::details::log_msg& msg);

	std::unique_ptr<spdlog::formatter> clone() const override;

	const std::string& getPattern() const noexcept { return pattern; }
	const std::chrono::time_zone* getTimeZone() const noexcept { return timeZone; }
	bool isColorEnabled() const noexcept { return colors; }

private:
	struct Impl;

	std::string pattern;
	const std::chrono::time_zone* timeZone;
	bool colors;
	std::unique_ptr<Impl> impl;
};

/**
 * A custom flag for spdlog patterns that prints the name of the logging thread (utils::concurrent::getCurrentThreadName), for sinks that use
 * spdlog pattern syntax instead of a PatternLayout:
 * <pre>
 * auto formatter = std::make_unique&lt;spdlog::pattern_formatter&gt;();
 * formatter-&gt;add_flag&lt;ThreadNameFlagFormatter&gt;('*').set_pattern("%H:%M:%S [%*] %v");
 * </pre>
 */
class ThreadNameFlagFormatter final : public spdlog::custom_flag_formatter {
public:
	void format(const spdlog::details::log_msg& msg, const std::tm& time, spdlog::memory_buf_t& dest) override;
	std::unique_ptr<custom_flag_formatter> clone() const override;
};

namespace detail {

/** Java: ch.qos.logback.classic.pattern.TargetLengthBasedClassNameAbbreviator.abbreviate */
std::string abbreviateLoggerName(std::string_view name, int32_t targetLength);

} // namespace detail

} // namespace aion::commons::logging
