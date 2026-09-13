#include "aion/commons/logging/PatternLayout.h"

#include <algorithm>
#include <climits>
#include <optional>
#include <cstdio>
#include <regex>
#include <vector>

#include <spdlog/details/os.h>

#include "aion/commons/logging/Logger.h"
#include "aion/commons/utils/DateTimeFormatter.h"
#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/commons/utils/concurrent/ThreadName.h"

namespace aion::commons::logging {

using utils::IllegalArgumentException;

namespace {

constexpr std::string_view SET_DEFAULT_COLOR = "\x1b[0;39m";
constexpr std::string_view ISO8601_PATTERN = "yyyy-MM-dd HH:mm:ss,SSS";
constexpr std::string_view LINE_SEPARATOR = spdlog::details::os::default_eol;

/** The data a converter formats */
struct Event {
	const spdlog::details::log_msg& msg;
	/** the message without the exception */
	std::string_view message;
	/** the exception text (lines separated by '\n'), empty if the message was not logged with an exception */
	std::string_view throwable;
};

/** Appends the exception text with the platform line separator after each line (Java: ThrowableProxyConverter uses CoreConstants.LINE_SEPARATOR) */
void appendThrowable(std::string_view throwable, std::string& out) {
	size_t start = 0;
	while (start < throwable.size()) {
		size_t end = throwable.find('\n', start);
		if (end == std::string_view::npos)
			end = throwable.size();
		std::string_view line = throwable.substr(start, end - start);
		if (line.ends_with('\r'))
			line.remove_suffix(1);
		out += line;
		out += LINE_SEPARATOR;
		start = end + 1;
	}
}

/** Java: ch.qos.logback.core.pattern.FormatInfo */
struct FormatInfo {
	int32_t min = INT_MIN;
	int32_t max = INT_MAX;
	bool leftPad = true;
	bool leftTruncate = true;
};

/** Java: ch.qos.logback.core.pattern.FormattingConverter */
class Converter {
public:
	virtual ~Converter() = default;

	virtual void convert(const Event& event, std::string& out) = 0;

	void write(const Event& event, std::string& out) {
		if (!formatInfo) {
			convert(event, out);
			return;
		}
		std::string s;
		convert(event, s);
		int32_t length = utils::StringUtils::utf16Length(s);
		if (length > formatInfo->max) {
			std::u16string utf16 = utils::StringUtils::toUtf16(s);
			size_t max = static_cast<size_t>(formatInfo->max);
			out += utils::StringUtils::toUtf8(formatInfo->leftTruncate ? std::u16string_view(utf16).substr(utf16.size() - max)
																															: std::u16string_view(utf16).substr(0, max));
		} else if (length < formatInfo->min) {
			size_t padding = static_cast<size_t>(formatInfo->min - length);
			if (formatInfo->leftPad)
				out.append(padding, ' ');
			out += s;
			if (!formatInfo->leftPad)
				out.append(padding, ' ');
		} else {
			out += s;
		}
	}

	std::optional<FormatInfo> formatInfo;
};

using ConverterList = std::vector<std::unique_ptr<Converter>>;

class LiteralConverter final : public Converter {
public:
	explicit LiteralConverter(std::string text) : text(std::move(text)) {}
	void convert(const Event&, std::string& out) override { out += text; }

private:
	std::string text;
};

/** Java: ch.qos.logback.classic.pattern.DateConverter. Caches the fields of the current second. */
class DateConverter final : public Converter {
public:
	DateConverter(utils::DateTimeFormatter formatter, const std::chrono::time_zone* timeZone) : formatter(std::move(formatter)), timeZone(timeZone) {}

	void convert(const Event& event, std::string& out) override {
		using namespace std::chrono;
		auto second = floor<seconds>(event.msg.time);
		if (second != cachedSecond) {
			fields = utils::DateTimeFields::of(event.msg.time, offsetAt(second));
			cachedSecond = second;
		} else {
			fields.nano = static_cast<int32_t>(duration_cast<nanoseconds>(event.msg.time - second).count());
		}
		formatter.formatTo(out, fields);
	}

private:
	std::chrono::seconds offsetAt(std::chrono::sys_seconds second) {
		if (!timeZone) // the C library determines the offset, cached per second by the caller
			return utils::getUtcOffset(second, nullptr);
		if (!(zoneInfo.begin <= second && second < zoneInfo.end)) // time zone database lookups are slow, the result is valid for a period
			zoneInfo = timeZone->get_info(second);
		return zoneInfo.offset;
	}

	utils::DateTimeFormatter formatter;
	const std::chrono::time_zone* timeZone;
	std::chrono::sys_info zoneInfo{};
	std::chrono::sys_seconds cachedSecond = std::chrono::sys_seconds::min();
	utils::DateTimeFields fields;
};

std::string_view levelName(spdlog::level::level_enum level) noexcept {
	switch (level) {
		case spdlog::level::trace:
			return "TRACE";
		case spdlog::level::debug:
			return "DEBUG";
		case spdlog::level::info:
			return "INFO";
		case spdlog::level::warn:
			return "WARN";
		case spdlog::level::err:
		case spdlog::level::critical:
			return "ERROR";
		default:
			return "OFF";
	}
}

class LevelConverter final : public Converter {
public:
	void convert(const Event& event, std::string& out) override { out += levelName(event.msg.level); }
};

class ThreadConverter final : public Converter {
public:
	void convert(const Event&, std::string& out) override { out += utils::concurrent::getCurrentThreadName(); }
};

/** Java: ch.qos.logback.classic.pattern.LoggerConverter */
class LoggerConverter final : public Converter {
public:
	explicit LoggerConverter(int32_t length) : length(length) {}

	void convert(const Event& event, std::string& out) override {
		std::string_view name(event.msg.logger_name.data(), event.msg.logger_name.size());
		if (length == 0) {
			size_t dot = name.rfind('.');
			out += dot == std::string_view::npos ? name : name.substr(dot + 1);
		} else if (length > 0) {
			out += detail::abbreviateLoggerName(name, length);
		} else {
			out += name;
		}
	}

private:
	int32_t length;
};

class MessageConverter final : public Converter {
public:
	void convert(const Event& event, std::string& out) override { out += event.message; }
};

class LineSeparatorConverter final : public Converter {
public:
	void convert(const Event&, std::string& out) override { out += LINE_SEPARATOR; }
};

/** Java: ch.qos.logback.classic.pattern.ThrowableProxyConverter - each line of the exception followed by a line separator, nothing without exception */
class ThrowableConverter final : public Converter {
public:
	void convert(const Event& event, std::string& out) override { appendThrowable(event.throwable, out); }
};

/** Java: NopThrowableInformationConverter */
class NopThrowableConverter final : public Converter {
public:
	void convert(const Event&, std::string&) override {}
};

class CompositeConverter : public Converter {
public:
	explicit CompositeConverter(ConverterList children) : children(std::move(children)) {}

protected:
	void writeChildren(const Event& event, std::string& out) {
		for (auto& child : children)
			child->write(event, out);
	}

private:
	ConverterList children;
};

class BareCompositeConverter final : public CompositeConverter {
public:
	using CompositeConverter::CompositeConverter;
	void convert(const Event& event, std::string& out) override { writeChildren(event, out); }
};

/** Java: ForegroundCompositeConverterBase and its subclasses, HighlightingCompositeConverter */
class ColorConverter final : public CompositeConverter {
public:
	/** @param colorCode ANSI color code, empty for %highlight */
	ColorConverter(ConverterList children, std::string_view colorCode, bool enabled)
		: CompositeConverter(std::move(children)), colorCode(colorCode), enabled(enabled) {}

	void convert(const Event& event, std::string& out) override {
		if (!enabled) {
			writeChildren(event, out);
			return;
		}
		out += "\x1b[";
		out += colorCode.empty() ? highlightColor(event.msg.level) : colorCode;
		out += 'm';
		writeChildren(event, out);
		out += SET_DEFAULT_COLOR;
	}

private:
	static std::string_view highlightColor(spdlog::level::level_enum level) noexcept {
		switch (level) {
			case spdlog::level::err:
			case spdlog::level::critical:
				return "1;31";
			case spdlog::level::warn:
				return "31";
			case spdlog::level::info:
				return "34";
			default:
				return "39";
		}
	}

	std::string_view colorCode;
	bool enabled;
};

/** Converts a Java regex replacement string (Matcher.replaceAll: $n group references, \ escapes) to ECMAScript format syntax. */
std::string toEcmaScriptReplacement(std::string_view replacement) {
	std::string result;
	for (size_t i = 0; i < replacement.size(); i++) {
		char c = replacement[i];
		if (c == '\\' && i + 1 < replacement.size()) {
			char escaped = replacement[++i];
			result += escaped == '$' ? "$$" : std::string(1, escaped);
		} else if (c == '$' && i + 1 < replacement.size() && replacement[i + 1] >= '0' && replacement[i + 1] <= '9') {
			result += '$';
		} else if (c == '$') {
			throw IllegalArgumentException("Illegal group reference in replacement: " + std::string(replacement));
		} else {
			result += c;
		}
	}
	return result;
}

/** Java: ch.qos.logback.core.pattern.ReplacingCompositeConverter */
class ReplacingConverter final : public CompositeConverter {
public:
	ReplacingConverter(ConverterList children, const std::string& regex, const std::string& replacement)
		: CompositeConverter(std::move(children)), regex(regex), replacement(toEcmaScriptReplacement(replacement)) {}

	void convert(const Event& event, std::string& out) override {
		std::string in;
		writeChildren(event, in);
		out += std::regex_replace(in, regex, replacement);
	}

private:
	std::regex regex;
	std::string replacement;
};

std::optional<std::string_view> colorCodeOf(std::string_view keyword) {
	static constexpr std::pair<std::string_view, std::string_view> COLORS[] = {
		{"black", "30"},        {"red", "31"},       {"green", "32"},      {"yellow", "33"},        {"blue", "34"},       {"magenta", "35"},
		{"cyan", "36"},         {"white", "37"},     {"gray", "1;30"},       {"boldRed", "1;31"},     {"boldGreen", "1;32"}, {"boldYellow", "1;33"},
		{"boldBlue", "1;34"},   {"boldMagenta", "1;35"}, {"boldCyan", "1;36"}, {"boldWhite", "1;37"}, {"highlight", ""},
	};
	for (const auto& [name, code] : COLORS) {
		if (name == keyword)
			return code;
	}
	return std::nullopt;
}

/** Parses a logback conversion pattern into converters (Java: ch.qos.logback.core.pattern.parser.Parser and Compiler) */
class Parser {
public:
	Parser(std::string_view pattern, const std::chrono::time_zone* timeZone, bool colors) : pattern(pattern), timeZone(timeZone), colors(colors) {}

	ConverterList parse() {
		ConverterList converters = parseSequence(false);
		if (position < pattern.size())
			fail("Unexpected ')'");
		return converters;
	}

	bool hasThrowableConverter() const noexcept { return throwableConverter; }

private:
	[[noreturn]] void fail(std::string_view problem) const {
		throw IllegalArgumentException(std::string(problem) + " at position " + std::to_string(position) + " of pattern \"" + std::string(pattern) + "\"");
	}

	bool at(char c) const noexcept { return position < pattern.size() && pattern[position] == c; }

	static bool isDigit(char c) noexcept { return c >= '0' && c <= '9'; }

	ConverterList parseSequence(bool nested) {
		ConverterList result;
		std::string literal;
		auto flushLiteral = [&] {
			if (!literal.empty()) {
				result.push_back(std::make_unique<LiteralConverter>(std::move(literal)));
				literal.clear();
			}
		};
		while (position < pattern.size()) {
			char c = pattern[position];
			if (c == '\\') {
				if (position + 1 >= pattern.size())
					fail("Incomplete escape sequence");
				char next = pattern[position + 1];
				switch (next) {
					case '%':
					case '(':
					case ')':
					case '{':
					case '}':
					case '\\':
						literal += next;
						break;
					case 't':
						literal += '\t';
						break;
					case 'n':
						literal += '\n';
						break;
					case 'r':
						literal += '\r';
						break;
					case '_':
						break;
					default:
						fail(std::string("Illegal escape sequence \\") + next);
				}
				position += 2;
			} else if (c == ')' && nested) {
				break;
			} else if (c != '%') {
				literal += c;
				position++;
			} else {
				flushLiteral();
				position++;
				result.push_back(parseConversion());
			}
		}
		flushLiteral();
		return result;
	}

	std::unique_ptr<Converter> parseConversion() {
		std::optional<FormatInfo> formatInfo = parseFormatInfo();
		std::unique_ptr<Converter> converter;
		if (at('(')) {
			converter = std::make_unique<BareCompositeConverter>(parseCompositeChildren());
			parseOptions();
		} else {
			std::string keyword = parseKeyword();
			if (keyword.empty())
				fail("Missing conversion word after %");
			if (at('(')) {
				ConverterList children = parseCompositeChildren();
				converter = createComposite(keyword, std::move(children), parseOptions());
			} else {
				converter = createSimple(keyword, parseOptions());
			}
		}
		if (!dynamic_cast<LiteralConverter*>(converter.get())) // logback does not apply format modifiers to the %PARSER_ERROR literal
			converter->formatInfo = formatInfo;
		return converter;
	}

	ConverterList parseCompositeChildren() {
		position++; // (
		ConverterList children = parseSequence(true);
		if (!at(')'))
			fail("Missing ')'");
		position++;
		return children;
	}

	int32_t parseNumber() {
		size_t start = position;
		int64_t value = 0;
		while (position < pattern.size() && isDigit(pattern[position])) {
			value = value * 10 + (pattern[position++] - '0');
			if (value > INT_MAX)
				fail("Number too large");
		}
		if (position == start)
			fail("Missing number in format modifier");
		return static_cast<int32_t>(value);
	}

	std::optional<FormatInfo> parseFormatInfo() {
		if (!(at('-') || at('.') || (position < pattern.size() && isDigit(pattern[position]))))
			return std::nullopt;
		FormatInfo info;
		if (at('-')) {
			info.leftPad = false;
			position++;
		}
		if (position < pattern.size() && isDigit(pattern[position]))
			info.min = parseNumber();
		if (at('.')) {
			position++;
			if (at('-')) {
				info.leftTruncate = false;
				position++;
			}
			info.max = parseNumber();
		}
		return info;
	}

	std::string parseKeyword() {
		size_t start = position;
		while (position < pattern.size()) {
			char c = pattern[position];
			if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || isDigit(c) || c == '_'))
				break;
			position++;
		}
		return std::string(pattern.substr(start, position - start));
	}

	/** Java: OptionTokenizer - {a, 'b, c', "d"}: comma separated, trimmed, optionally quoted values */
	std::vector<std::string> parseOptions() {
		std::vector<std::string> options;
		while (at('{')) {
			position++;
			std::string token;
			bool quotedToken = false;
			char quote = 0;
			auto emit = [&] {
				if (quotedToken) {
					options.push_back(std::move(token));
				} else {
					std::string_view trimmed = utils::StringUtils::trim(token);
					if (!trimmed.empty())
						options.emplace_back(trimmed);
				}
				token.clear();
				quotedToken = false;
			};
			while (true) {
				if (position >= pattern.size())
					fail("Missing '}'");
				char c = pattern[position++];
				if (quote) {
					if (c == '\\' && position < pattern.size() && pattern[position] == quote)
						token += pattern[position++];
					else if (c == quote)
						quote = 0;
					else
						token += c;
				} else if ((c == '\'' || c == '"') && utils::StringUtils::trim(token).empty()) {
					quote = c;
					quotedToken = true;
					token.clear();
				} else if (c == ',') {
					emit();
				} else if (c == '}') {
					emit();
					break;
				} else if (!quotedToken) {
					token += c;
				}
			}
		}
		return options;
	}

	std::unique_ptr<Converter> createSimple(std::string_view keyword, const std::vector<std::string>& options) {
		if (keyword == "d" || keyword == "date") {
			std::string_view datePattern = options.empty() || options[0] == "ISO8601" ? ISO8601_PATTERN : std::string_view(options[0]);
			const std::chrono::time_zone* zone = options.size() > 1 ? utils::findTimeZone(options[1]) : timeZone;
			return std::make_unique<DateConverter>(utils::DateTimeFormatter::ofPattern(datePattern), zone);
		}
		if (keyword == "level" || keyword == "le" || keyword == "p")
			return std::make_unique<LevelConverter>();
		if (keyword == "thread" || keyword == "t")
			return std::make_unique<ThreadConverter>();
		if (keyword == "logger" || keyword == "lo" || keyword == "c") {
			int32_t length = -1;
			if (!options.empty()) {
				try {
					length = std::stoi(options[0]);
				} catch (const std::exception&) {
					fail("Invalid logger name length \"" + options[0] + "\"");
				}
			}
			return std::make_unique<LoggerConverter>(length);
		}
		if (keyword == "message" || keyword == "msg" || keyword == "m")
			return std::make_unique<MessageConverter>();
		if (keyword == "n")
			return std::make_unique<LineSeparatorConverter>();
		if (keyword == "ex" || keyword == "exception" || keyword == "throwable") {
			throwableConverter = true;
			return std::make_unique<ThrowableConverter>();
		}
		if (keyword == "nopex" || keyword == "nopexception") {
			throwableConverter = true;
			return std::make_unique<NopThrowableConverter>();
		}
		return parserError("[" + std::string(keyword) + "] is not a valid conversion word", keyword);
	}

	std::unique_ptr<Converter> createComposite(std::string_view keyword, ConverterList children, const std::vector<std::string>& options) {
		if (auto colorCode = colorCodeOf(keyword))
			return std::make_unique<ColorConverter>(std::move(children), *colorCode, colors);
		if (keyword == "replace") {
			if (options.size() < 2)
				fail("%replace requires a regular expression and a replacement");
			try {
				return std::make_unique<ReplacingConverter>(std::move(children), options[0], options[1]);
			} catch (const std::regex_error& e) {
				fail("Invalid regular expression \"" + options[0] + "\": " + e.what());
			}
		}
		return parserError("Failed to create converter for [%" + std::string(keyword) + "] keyword", keyword);
	}

	/**
	 * Java: Compiler - an unknown conversion word does not stop the layout: it is replaced by the literal "%PARSER_ERROR[word]" and the error is
	 * reported (logback: as an error status, printed on the console).
	 */
	std::unique_ptr<Converter> parserError(const std::string& problem, std::string_view keyword) {
		std::fprintf(stderr, "ERROR in PatternLayout(\"%s\") - %s\n", std::string(pattern).c_str(), problem.c_str());
		return std::make_unique<LiteralConverter>("%PARSER_ERROR[" + std::string(keyword) + "]");
	}

	std::string_view pattern;
	const std::chrono::time_zone* timeZone;
	bool colors;
	size_t position = 0;
	bool throwableConverter = false;
};

} // namespace

struct PatternLayout::Impl {
	ConverterList converters;
	bool handlesThrowable = false;
	std::string buffer;
};

PatternLayout::PatternLayout(std::string_view pattern, const std::chrono::time_zone* timeZone, bool enableColors)
	: pattern(pattern), timeZone(timeZone), colors(enableColors), impl(std::make_unique<Impl>()) {
	Parser parser(this->pattern, timeZone, enableColors);
	impl->converters = parser.parse();
	impl->handlesThrowable = parser.hasThrowableConverter();
}

PatternLayout::~PatternLayout() = default;

void PatternLayout::format(const spdlog::details::log_msg& msg, spdlog::memory_buf_t& dest) {
	std::string& out = impl->buffer;
	out.clear();
	std::string_view text(msg.payload.data(), msg.payload.size());
	Event event{msg, text, {}};
	if (size_t throwableStart = detail::getThrowableStart(msg); throwableStart <= text.size()) {
		event.throwable = text.substr(throwableStart);
		event.message = text.substr(0, throwableStart > 0 ? throwableStart - 1 : 0); // without the '\n' between message and exception
	}
	for (auto& converter : impl->converters)
		converter->write(event, out);
	if (!impl->handlesThrowable) // Java: PatternLayout's EnsureExceptionHandling appends %xEx to patterns without a throwable converter
		appendThrowable(event.throwable, out);
	dest.append(out.data(), out.data() + out.size());
}

std::string PatternLayout::format(const spdlog::details::log_msg& msg) {
	spdlog::memory_buf_t buffer;
	format(msg, buffer);
	return std::string(buffer.data(), buffer.size());
}

std::unique_ptr<spdlog::formatter> PatternLayout::clone() const {
	return std::make_unique<PatternLayout>(pattern, timeZone, colors);
}

void ThreadNameFlagFormatter::format(const spdlog::details::log_msg&, const std::tm&, spdlog::memory_buf_t& dest) {
	const std::string& name = utils::concurrent::getCurrentThreadName();
	dest.append(name.data(), name.data() + name.size());
}

std::unique_ptr<spdlog::custom_flag_formatter> ThreadNameFlagFormatter::clone() const {
	return std::make_unique<ThreadNameFlagFormatter>();
}

namespace detail {

std::string abbreviateLoggerName(std::string_view name, int32_t targetLength) {
	if (static_cast<int64_t>(name.size()) < targetLength)
		return std::string(name);
	std::vector<size_t> dotIndexes;
	for (size_t i = name.find('.'); i != std::string_view::npos; i = name.find('.', i + 1))
		dotIndexes.push_back(i);
	if (dotIndexes.empty()) // abbreviation is not possible
		return std::string(name);
	// computeLengthArray: from the left, segments are shortened to their first character until the name is short enough
	int64_t toTrim = static_cast<int64_t>(name.size()) - targetLength;
	std::string result;
	size_t segmentStart = 0;
	for (size_t dotIndex : dotIndexes) {
		int64_t available = static_cast<int64_t>(dotIndex - segmentStart);
		int64_t length = toTrim > 0 ? std::min<int64_t>(available, 1) : available;
		toTrim -= available - length;
		result += name.substr(segmentStart, static_cast<size_t>(length));
		result += '.';
		segmentStart = dotIndex + 1;
	}
	result += name.substr(segmentStart); // the last segment is never shortened
	return result;
}

} // namespace detail

} // namespace aion::commons::logging
