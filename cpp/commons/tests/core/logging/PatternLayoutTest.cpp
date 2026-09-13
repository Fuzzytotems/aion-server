#include <gtest/gtest.h>

#include <functional>
#include <sstream>
#include <stdexcept>
#include <thread>

#include <spdlog/details/log_msg.h>
#include <spdlog/details/os.h>
#include <spdlog/sinks/ostream_sink.h>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/logging/Logging.h"
#include "aion/commons/logging/PatternLayout.h"
#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/commons/utils/concurrent/ThreadName.h"

using namespace aion::commons;
using namespace aion::commons::logging;
using namespace std::chrono;

namespace {

const std::string EOL = spdlog::details::os::default_eol;

/** 2026-09-12T13:42:30.123Z */
constexpr auto SAMPLE_TIME = sys_days(2026y / September / 12) + 13h + 42min + 30s + 123ms;

spdlog::details::log_msg message(std::string_view text, spdlog::level::level_enum level = spdlog::level::info,
	std::string_view loggerName = "com.aionemu.gameserver.services.MailService") {
	spdlog::details::log_msg msg(spdlog::source_loc{}, spdlog::string_view_t(loggerName.data(), loggerName.size()), level,
		spdlog::string_view_t(text.data(), text.size()));
	msg.time = time_point_cast<system_clock::duration>(SAMPLE_TIME);
	return msg;
}

const time_zone* utcOrSkip() {
	try {
		return locate_zone("UTC");
	} catch (const std::exception&) {
		return nullptr;
	}
}

/** Formats on a thread named "main" (the thread name is part of most patterns) */
std::string layoutFormat(std::string_view pattern, const spdlog::details::log_msg& msg, bool colors = false, const time_zone* zone = nullptr) {
	std::string result;
	std::thread([&] {
		utils::concurrent::setCurrentThreadName("main");
		PatternLayout layout(pattern, zone, colors);
		result = layout.format(msg);
	}).join();
	return result;
}

/** Logs through a Logger (which passes exceptions separately from the message) to a sink with the layout, on a thread named "main" */
std::string logWithLayout(std::string_view pattern, const std::function<void(const Logger&)>& logging) {
	std::ostringstream stream;
	auto sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(stream);
	sink->set_formatter(std::make_unique<PatternLayout>(pattern, nullptr, false));
	const std::string name = "test.PatternLayoutTest.MailService";
	LoggerFactory::configure(name, {.level = spdlog::level::trace, .sinks = {sink}, .additive = false});
	std::thread([&] {
		utils::concurrent::setCurrentThreadName("main");
		logging(LoggerFactory::getLogger(name));
	}).join();
	LoggerFactory::removeConfig(name);
	return stream.str();
}

/** The exception text with the platform line separator after each line, like logback prints it */
std::string throwableText(const std::exception& e) {
	return utils::StringUtils::replace(utils::toStackTraceString(e), "\n", EOL) + EOL;
}

} // namespace

TEST(PatternLayoutTest, ServerPatterns) {
	const time_zone* utc = utcOrSkip();
	if (!utc)
		GTEST_SKIP() << "no time zone database available";
	auto msg = message("Mail sent", spdlog::level::warn);
	EXPECT_EQ(layoutFormat(Logging::resolvePattern(Logging::CONSOLE_PATTERN), msg, true, utc),
		"13:42:30 \x1b[31mWARN \x1b[0;39m \x1b[1;30m[main]\x1b[0;39m - Mail sent" + EOL); // logback GrayCompositeConverter: BOLD + BLACK_FG
	EXPECT_EQ(layoutFormat(Logging::resolvePattern(Logging::CONSOLE_PATTERN), msg, false, utc), "13:42:30 WARN  [main] - Mail sent" + EOL);
	EXPECT_EQ(layoutFormat(Logging::resolvePattern(Logging::SERVER_CONSOLE_FILE_PATTERN), msg, false, utc),
		"2026-09-12T13:42:30,123Z WARN  [main] com.aionemu.gameserver.services.MailService - Mail sent" + EOL);
	EXPECT_EQ(layoutFormat(Logging::resolvePattern(Logging::WARNINGS_AND_ERRORS_FILE_PATTERN), msg, false, utc),
		"2026-09-12T13:42:30,123Z com.aionemu.gameserver.services.MailService - Mail sent" + EOL);
	EXPECT_EQ(layoutFormat(Logging::resolvePattern("${date} %message%n"), message("audit"), false, utc), "2026-09-12T13:42:30,123Z audit" + EOL);
}

TEST(PatternLayoutTest, TimeZones) {
	const time_zone* berlin = nullptr;
	try {
		berlin = locate_zone("Europe/Berlin");
	} catch (const std::exception&) {
		GTEST_SKIP() << "no time zone database available";
	}
	EXPECT_EQ(layoutFormat(Logging::DATE_PATTERN, message("x"), false, berlin), "2026-09-12T15:42:30,123+02:00");
	EXPECT_EQ(layoutFormat("%date{HH:mm, Asia/Tokyo}", message("x"), false, berlin), "22:42");
	EXPECT_EQ(layoutFormat("%d", message("x"), false, berlin), "2026-09-12 15:42:30,123");
	EXPECT_EQ(layoutFormat("%date{ISO8601}", message("x"), false, berlin), "2026-09-12 15:42:30,123");
	EXPECT_THROW(PatternLayout("%date{HH, Nowhere/Nothing}"), utils::IllegalArgumentException);

	// the system default zone works without the time zone database
	auto now = message("x");
	now.time = system_clock::now();
	EXPECT_EQ(layoutFormat("%date{yyyy-MM-dd HH:mm:ss}", now).size(), 19u);
}

TEST(PatternLayoutTest, LevelsAndHighlightColors) {
	EXPECT_EQ(layoutFormat("%level", message("", spdlog::level::trace)), "TRACE");
	EXPECT_EQ(layoutFormat("%le", message("", spdlog::level::debug)), "DEBUG");
	EXPECT_EQ(layoutFormat("%p", message("", spdlog::level::info)), "INFO");
	EXPECT_EQ(layoutFormat("%level", message("", spdlog::level::err)), "ERROR");
	EXPECT_EQ(layoutFormat("%level", message("", spdlog::level::critical)), "ERROR");
	EXPECT_EQ(layoutFormat("%highlight(x)", message("", spdlog::level::err), true), "\x1b[1;31mx\x1b[0;39m");
	EXPECT_EQ(layoutFormat("%highlight(x)", message("", spdlog::level::info), true), "\x1b[34mx\x1b[0;39m");
	EXPECT_EQ(layoutFormat("%highlight(x)", message("", spdlog::level::debug), true), "\x1b[39mx\x1b[0;39m");
	EXPECT_EQ(layoutFormat("%boldGreen(x)%cyan(y)", message(""), true), "\x1b[1;32mx\x1b[0;39m\x1b[36my\x1b[0;39m");
}

TEST(PatternLayoutTest, LoggerNameAbbreviation) {
	auto msg = message("", spdlog::level::info, "mainPackage.sub.sample.Bar");
	// the examples of the logback documentation
	EXPECT_EQ(layoutFormat("%logger", msg), "mainPackage.sub.sample.Bar");
	EXPECT_EQ(layoutFormat("%logger{0}", msg), "Bar");
	EXPECT_EQ(layoutFormat("%logger{5}", msg), "m.s.s.Bar");
	EXPECT_EQ(layoutFormat("%logger{10}", msg), "m.s.s.Bar");
	EXPECT_EQ(layoutFormat("%logger{15}", msg), "m.s.sample.Bar");
	EXPECT_EQ(layoutFormat("%logger{16}", msg), "m.sub.sample.Bar");
	EXPECT_EQ(layoutFormat("%c{26}", msg), "mainPackage.sub.sample.Bar");
	EXPECT_EQ(layoutFormat("%lo{0}", message("", spdlog::level::info, "CHAT_LOG")), "CHAT_LOG");
}

TEST(PatternLayoutTest, FormatModifiers) {
	auto msg = message("", spdlog::level::info, "a.b.Logger");
	EXPECT_EQ(layoutFormat("[%-6level]", msg), "[INFO  ]");
	EXPECT_EQ(layoutFormat("[%6level]", msg), "[  INFO]");
	EXPECT_EQ(layoutFormat("[%.2thread]", msg), "[in]");  // truncated from the left
	EXPECT_EQ(layoutFormat("[%.-2thread]", msg), "[ma]"); // truncated from the right
	EXPECT_EQ(layoutFormat("[%-8.10logger{0}]", msg), "[Logger  ]");
	EXPECT_EQ(layoutFormat("[%-12(%level %thread)]", msg), "[INFO main   ]");
	EXPECT_EQ(layoutFormat("[%3.3message]", message("Grüße")), "[üße]"); // UTF-16 based widths
}

TEST(PatternLayoutTest, EscapesAndLiterals) {
	EXPECT_EQ(layoutFormat("100\\% \\(x\\) \\{\\} a\\_b\\\\", message("")), "100% (x) {} ab\\");
	EXPECT_EQ(layoutFormat("a\\tb", message("")), "a\tb");
	EXPECT_EQ(layoutFormat("%m%n", message("x")), "x" + EOL);
	EXPECT_EQ(layoutFormat("no conversions", message("x")), "no conversions");
}

TEST(PatternLayoutTest, InvalidPatterns) {
	EXPECT_THROW(PatternLayout("%highlight(%level"), utils::IllegalArgumentException);
	EXPECT_THROW(PatternLayout("%date{yyyy"), utils::IllegalArgumentException);
	EXPECT_THROW(PatternLayout("%"), utils::IllegalArgumentException);
	EXPECT_THROW(PatternLayout("\\q"), utils::IllegalArgumentException);
	EXPECT_THROW(PatternLayout("%replace(%msg){onlyRegex}"), utils::IllegalArgumentException);
	EXPECT_THROW(PatternLayout("%replace(%msg){'([', 'x'}"), utils::IllegalArgumentException);
}

TEST(PatternLayoutTest, UnknownConversionWordsArePrintedAsParserErrors) {
	// logback's Compiler keeps the layout working and prints %PARSER_ERROR[word] (and reports the error)
	EXPECT_EQ(layoutFormat("a %unknown b", message("")), "a %PARSER_ERROR[unknown] b");
	EXPECT_EQ(layoutFormat("[%-20foo(%msg){x}]", message("text")), "[%PARSER_ERROR[foo]]");
	// e.g. a percent-encoded avatar URL in the status Discord pattern: format modifier 20 and the unknown word "avatar"
	std::string pattern = utils::StringUtils::replace(Logging::STATUS_DISCORD_PATTERN, "${avatarUrl}", "https://cdn/aion%20avatar.png");
	EXPECT_EQ(layoutFormat(pattern, message("Player count is 0", spdlog::level::warn)), "MailService [main]|https://cdn/aion%PARSER_ERROR[avatar].png|Player count is 0");
}

TEST(PatternLayoutTest, ChatServerDiscordPattern) {
	// chat server logback.xml: %replace(%msg){'\[(.*?) \((.)\)\] (.*?): (.*)', '[$1] $3|${chatserver.log.chat.discord.avatar_url}|$4'}
	std::string pattern = R"(%replace(%msg){'\[(.*?) \((.)\)\] (.*?): (.*)', '[$1] $3|https://server/avatar_$2.png|$4'})";
	EXPECT_EQ(layoutFormat(pattern, message("[Heal Bot (E)] LFG: need a healer: now")), "[Heal Bot] LFG|https://server/avatar_E.png|need a healer: now");
	EXPECT_EQ(layoutFormat(pattern, message("no match")), "no match");
	EXPECT_EQ(layoutFormat("%replace(%msg){'a', '\\$'}", message("banana")), "b$n$n$");
}

TEST(PatternLayoutTest, StatusDiscordPatternSeparatesExceptions) {
	std::string pattern = utils::StringUtils::replace(Logging::STATUS_DISCORD_PATTERN, "${avatarUrl}", "https://server/%level.png");
	EXPECT_EQ(layoutFormat(pattern, message("Player count is 0", spdlog::level::warn)), "MailService [main]|https://server/WARN.png|Player count is 0");

	utils::IllegalStateException exception("mailbox full", std::make_exception_ptr(std::runtime_error("disk")));
	std::string output = logWithLayout(pattern, [&](const Logger& log) { log.error("Could not send mail", exception); });
	EXPECT_EQ(output, "MailService [main]|https://server/ERROR.png|Could not send mail" + EOL + "```qml" + EOL + throwableText(exception) + "```");
	EXPECT_NE(output.find("Caused by: std::runtime_error: disk" + EOL), std::string::npos);
}

TEST(PatternLayoutTest, ExceptionsAreSeparateFromTheMessage) {
	std::runtime_error exception("bad index");
	// Java: %xEx is appended to patterns without a throwable converter, after the line separator of the pattern
	EXPECT_EQ(logWithLayout("%level - %msg%n", [&](const Logger& log) { log.error("Failure", exception); }),
		"ERROR - Failure" + EOL + "std::runtime_error: bad index" + EOL);
	EXPECT_EQ(logWithLayout("%level - %msg%n", [&](const Logger& log) { log.warn("", exception); }), "WARN - " + EOL + "std::runtime_error: bad index" + EOL);
	EXPECT_EQ(logWithLayout("[%msg][%ex]", [&](const Logger& log) { log.info("Failure", exception); }), "[Failure][std::runtime_error: bad index" + EOL + "]");
	EXPECT_EQ(logWithLayout("[%msg][%nopex]", [&](const Logger& log) { log.info("Failure", exception); }), "[Failure][]");
	EXPECT_EQ(logWithLayout("[%msg]", [&](const Logger& log) { log.info("Failure {}", 7, exception); }), "[Failure 7]std::runtime_error: bad index" + EOL);
	EXPECT_EQ(logWithLayout("[%msg]", [&](const Logger& log) {
		try {
			throw 42;
		} catch (...) {
			log.errorCurrentException("Unknown");
		}
	}), "[Unknown]<unknown exception type>" + EOL);

	// messages without an exception are never split, even if a line looks like an exception
	EXPECT_EQ(logWithLayout("[%msg][%ex]", [&](const Logger& log) { log.info("text\nstd::out_of_range: bad index"); }), "[text\nstd::out_of_range: bad index][]");
	EXPECT_EQ(layoutFormat("[%msg][%ex]", message("std::out_of_range: bad index")), "[std::out_of_range: bad index][]");
	EXPECT_EQ(layoutFormat("%msg", message("plain")), "plain");
}

TEST(PatternLayoutTest, CloneAndSpdlogThreadNameFlag) {
	PatternLayout layout("%level %m", nullptr, false);
	auto copy = layout.clone();
	spdlog::memory_buf_t buffer;
	copy->format(message("cloned", spdlog::level::warn), buffer);
	EXPECT_EQ(std::string(buffer.data(), buffer.size()), "WARN cloned");

	std::string formatted;
	std::thread([&] {
		utils::concurrent::setCurrentThreadName("Worker-7");
		auto formatter = Logging::createSpdlogFormatter("[%*] %v");
		spdlog::memory_buf_t out;
		formatter->format(message("spdlog"), out);
		formatted = std::string(out.data(), out.size());
	}).join();
	EXPECT_EQ(formatted, "[Worker-7] spdlog" + EOL);
}
