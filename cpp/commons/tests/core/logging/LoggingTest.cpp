#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <stdexcept>

#include <spdlog/details/os.h>

#include "aion/commons/logging/Logging.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/DateTimeFormatter.h"
#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/info/SystemInfo.h"

using namespace aion::commons;
using namespace aion::commons::logging;
namespace fs = std::filesystem;

namespace {

const std::string EOL = spdlog::details::os::default_eol;

class TempFolder {
public:
	explicit TempFolder(std::string_view name) : path(fs::temp_directory_path() / name) { fs::remove_all(path); }
	~TempFolder() {
		std::error_code ignored;
		fs::remove_all(path, ignored);
	}
	const fs::path path;
};

std::string readFile(const fs::path& file) {
	std::ifstream in(file, std::ios::binary);
	std::ostringstream content;
	content << in.rdbuf();
	return content.str();
}

std::vector<std::string> lines(const std::string& text) {
	std::vector<std::string> result;
	std::istringstream in(text);
	for (std::string line; std::getline(in, line);) {
		if (line.ends_with('\r'))
			line.pop_back();
		result.push_back(line);
	}
	return result;
}

void writeFile(const fs::path& file, std::string_view content, fs::file_time_type modificationTime) {
	fs::create_directories(file.parent_path());
	std::ofstream(file, std::ios::binary) << content;
	fs::last_write_time(file, modificationTime);
}

fs::file_time_type fileTime(std::chrono::sys_seconds time) {
	return std::chrono::clock_cast<fs::file_time_type::clock>(time);
}

/** Restores the default logging state (root logger to stderr) even if a test fails */
struct LoggingGuard {
	~LoggingGuard() {
		Logging::shutdown();
		LoggerFactory::setRootLevel(spdlog::level::info);
	}
};

} // namespace

TEST(LoggingTest, InitCreatesRootAppenders) {
	TempFolder folder("aion_LoggingTest_init");
	{
		LoggingGuard guard;
		Logging::init({.logFolder = folder.path, .console = false});
		auto log = LoggerFactory::getLogger("com.aionemu.test.LoggingTest");
		log.debug("not logged");
		log.info("Server started");
		log.warn("Low memory");
		log.error("Failure", utils::IllegalStateException("broken"));
		log.info("After");
	} // shutdown closes the files

	auto console = lines(readFile(folder.path / "server_console.log"));
	ASSERT_GE(console.size(), 5u);
	std::regex line(R"(^\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d,\d{3}(Z|[+-]\d\d:\d\d) (INFO |WARN |ERROR) \[[^\]]+\] com\.aionemu\.test\.LoggingTest - .*$)");
	EXPECT_TRUE(std::regex_match(console[0], line)) << console[0];
	EXPECT_TRUE(console[0].ends_with(" - Server started")) << console[0];
	EXPECT_TRUE(console[1].ends_with(" - Low memory")) << console[1];
	EXPECT_TRUE(console[2].ends_with(" - Failure")) << console[2];
	EXPECT_EQ(console[3], "aion::commons::utils::IllegalStateException: broken");
	EXPECT_TRUE(console.back().ends_with(" - After"));
	EXPECT_EQ(readFile(folder.path / "server_console.log").find("not logged"), std::string::npos);

	auto warnings = lines(readFile(folder.path / "server_warnings.log"));
	ASSERT_EQ(warnings.size(), 1u);
	EXPECT_TRUE(std::regex_match(warnings[0], std::regex(R"(^\S+ com\.aionemu\.test\.LoggingTest - Low memory$)"))) << warnings[0];

	auto errors = lines(readFile(folder.path / "server_errors.log"));
	ASSERT_GE(errors.size(), 2u);
	EXPECT_TRUE(errors[0].ends_with(" com.aionemu.test.LoggingTest - Failure")) << errors[0];
	EXPECT_EQ(errors[1], "aion::commons::utils::IllegalStateException: broken");
	EXPECT_EQ(readFile(folder.path / "server_errors.log").find("Low memory"), std::string::npos);
}

TEST(LoggingTest, ServerSpecificAppenders) {
	TempFolder folder("aion_LoggingTest_appenders");
	{
		LoggingGuard guard;
		Logging::init({.logFolder = folder.path, .rootLevel = spdlog::level::warn, .console = false});
		// game server logback.xml: <logger name="ITEM_LOG" additivity="false"> and <logger name="AUDIT_LOG"> (additive)
		LoggerFactory::configure("ITEM_LOG", {.level = spdlog::level::info, .sinks = {Logging::createFileAppender("item.log", "${date} %message%n", std::nullopt, false)}, .additive = false});
		LoggerFactory::configure("AUDIT_LOG", {.level = spdlog::level::info, .sinks = {Logging::createFileAppender("audit.log", "%message%n")}});
		LoggerFactory::getLogger("ITEM_LOG").info("item created");
		LoggerFactory::getLogger("AUDIT_LOG").warn("suspicious");
		LoggerFactory::getLogger("com.aionemu.Other").info("below root level");
		LoggerFactory::removeConfig("ITEM_LOG");
		LoggerFactory::removeConfig("AUDIT_LOG");
	}
	EXPECT_TRUE(std::regex_match(readFile(folder.path / "item.log"), std::regex(R"(^\S+ item created\r?\n$)"))) << readFile(folder.path / "item.log");
	EXPECT_EQ(readFile(folder.path / "audit.log"), "suspicious" + EOL);
	std::string console = readFile(folder.path / "server_console.log");
	EXPECT_EQ(console.find("item created"), std::string::npos); // not additive
	EXPECT_NE(console.find("suspicious"), std::string::npos);   // additive
	EXPECT_EQ(console.find("below root level"), std::string::npos);
	EXPECT_EQ(Logging::createDiscordAppender("app_chat_discord_async", "", "%msg"), nullptr);
}

TEST(LoggingTest, ReinitializingArchivesFilesOfOpenConfigAppenders) {
	TempFolder folder("aion_LoggingTest_reinit");
	{
		LoggingGuard guard;
		Logging::init({.logFolder = folder.path, .console = false});
		LoggerFactory::configure("test.reinit.ITEM_LOG", {.sinks = {Logging::createFileAppender("item.log", "%message%n")}, .additive = false});
		LoggerFactory::getLogger("test.reinit.ITEM_LOG").info("before reinit");
		EXPECT_NO_THROW(Logging::init({.logFolder = folder.path, .console = false})); // archives item.log while the appender still has it open
		LoggerFactory::removeConfig("test.reinit.ITEM_LOG");
	}
	EXPECT_EQ(std::distance(fs::directory_iterator(folder.path / "archived"), fs::directory_iterator()), 1);
	EXPECT_FALSE(fs::exists(folder.path / "item.log"));
}

TEST(LoggingTest, PropertyKeys) {
	EXPECT_EQ(Logging::getPropertyKeys("gameserver"),
		(std::vector<std::string>{"gameserver.log.status.discord.webhook_url", "gameserver.log.status.discord.avatar_url"}));
}

TEST(LoggingTest, ExceptionsInLogFilesLikeLogback) {
	TempFolder folder("aion_LoggingTest_exceptions");
	{
		LoggingGuard guard;
		Logging::init({.logFolder = folder.path, .console = false});
		LoggerFactory::getLogger("com.aionemu.test.LoggingTest").error("", std::runtime_error("broken"));
	}
	// the exception follows the pattern's line separator, every line ends with the platform line separator
	EXPECT_TRUE(readFile(folder.path / "server_errors.log").ends_with(" com.aionemu.test.LoggingTest - " + EOL + "std::runtime_error: broken" + EOL))
		<< readFile(folder.path / "server_errors.log");
}

#if GTEST_HAS_DEATH_TEST
TEST(LoggingDeathTest, ExitWithoutShutdownStopsDiscordAppender) {
	// the Discord worker logs an error while the process exits without Logging::shutdown (used to run during static destruction)
	GTEST_FLAG_SET(death_test_style, "threadsafe");
	TempFolder folder("aion_LoggingTest_exit");
	auto exitWithoutShutdown = [&] {
		Logging::init({.logFolder = folder.path, .statusDiscordWebhookUrl = "http://127.0.0.1:9/api/webhooks/1/token", .console = false});
		LoggerFactory::getLogger("com.aionemu.test.LoggingTest").warn("queued for Discord");
		std::fprintf(stderr, "exiting\n");
		std::exit(0);
	};
	EXPECT_EXIT(exitWithoutShutdown(), testing::ExitedWithCode(0), "exiting");
	EXPECT_NE(readFile(folder.path / "server_warnings.log").find("Error sending Discord message: "), std::string::npos)
		<< readFile(folder.path / "server_warnings.log"); // sent (and failed) before the process exited
}
#endif

TEST(LoggingTest, ArchivesLogsOfPreviousRun) {
	using namespace std::chrono;
	TempFolder folder("aion_LoggingTest_archive");
	auto lastStart = sys_days(2026y / August / 1) + 10h + 5min;
	auto lastStop = sys_days(2026y / August / 3) + 22h + 30min + 59s;
	writeFile(folder.path / "[server_start_marker]", "", fileTime(lastStart));
	writeFile(folder.path / "server_console.log", "console content", fileTime(lastStop - 1h));
	writeFile(folder.path / "chat.LOG", "chat content", fileTime(lastStop));
	writeFile(folder.path / "stats" / "MethodStats.log", "<entries/>", fileTime(lastStart));
	writeFile(folder.path / "notes.txt", "kept", fileTime(lastStart));

	Logging::archiveLogs(folder.path);

	auto format = utils::DateTimeFormatter::ofPattern("yyyy-MM-dd HH.mm");
	fs::path archive = folder.path / "archived" / (format.format(lastStart) + " to " + format.format(lastStop) + ".zip");
	EXPECT_TRUE(fs::exists(archive)) << archive;
	EXPECT_FALSE(fs::exists(folder.path / "server_console.log"));
	EXPECT_FALSE(fs::exists(folder.path / "chat.LOG"));
	EXPECT_FALSE(fs::exists(folder.path / "stats")); // emptied folders are removed
	EXPECT_TRUE(fs::exists(folder.path / "notes.txt"));

	std::string zip = readFile(archive);
	EXPECT_NE(zip.find("stats/MethodStats.log"), std::string::npos);
	EXPECT_NE(zip.find("server_console.log"), std::string::npos);

	auto markerTime = floor<milliseconds>(clock_cast<system_clock>(fs::last_write_time(folder.path / "[server_start_marker]")));
	EXPECT_EQ(markerTime, floor<milliseconds>(utils::info::SystemInfo::getProcessStartTime()));

	// next start: no log files, nothing to archive
	Logging::archiveLogs(folder.path);
	EXPECT_EQ(std::distance(fs::directory_iterator(folder.path / "archived"), fs::directory_iterator()), 1);
}

TEST(LoggingTest, ArchiveWithoutStartMarker) {
	TempFolder folder("aion_LoggingTest_unknown");
	auto lastStop = std::chrono::sys_days(std::chrono::January / 2 / 2026) + std::chrono::hours(3);
	writeFile(folder.path / "server_errors.log", "x", fileTime(lastStop));
	{
		LoggingGuard guard;
		Logging::init({.logFolder = folder.path, .console = false}); // init archives, then creates new log files
	}
	auto format = utils::DateTimeFormatter::ofPattern("yyyy-MM-dd HH.mm");
	EXPECT_TRUE(fs::exists(folder.path / "archived" / ("Unknown to " + format.format(lastStop) + ".zip")));
	EXPECT_TRUE(fs::exists(folder.path / "[server_start_marker]"));
	EXPECT_TRUE(fs::exists(folder.path / "server_console.log"));
}

TEST(LoggingTest, ArchiveErrorsAreWrapped) {
	TempFolder folder("aion_LoggingTest_error");
	writeFile(folder.path / "file", "not a folder", fs::file_time_type::clock::now());
	try {
		Logging::archiveLogs(folder.path / "file");
		FAIL() << "no exception";
	} catch (const utils::Exception& e) {
		EXPECT_STREQ(e.what(), "Error gathering and archiving old logs");
		EXPECT_TRUE(e.cause());
	}
}
