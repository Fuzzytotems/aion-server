#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>

#include <spdlog/details/os.h>

#include "aion/commons/logging/FileAppender.h"
#include "aion/commons/logging/PatternLayout.h"
#include "aion/commons/utils/Exception.h"

using namespace aion::commons;
using namespace aion::commons::logging;

namespace {

const std::string EOL = spdlog::details::os::default_eol;

class TempFolder {
public:
	explicit TempFolder(std::string_view name) : path(std::filesystem::temp_directory_path() / name) { std::filesystem::remove_all(path); }
	~TempFolder() {
		std::error_code ignored;
		std::filesystem::remove_all(path, ignored);
	}
	const std::filesystem::path path;
};

std::string readFile(const std::filesystem::path& file) {
	std::ifstream in(file, std::ios::binary);
	std::ostringstream content;
	content << in.rdbuf();
	return content.str();
}

void log(spdlog::sinks::sink& sink, std::string_view text, spdlog::level::level_enum level) {
	spdlog::details::log_msg msg(spdlog::source_loc{}, "test.Logger", level, spdlog::string_view_t(text.data(), text.size()));
	sink.log(msg);
}

} // namespace

TEST(FileAppenderTest, AppendsFormattedAndFilteredMessages) {
	TempFolder folder("aion_FileAppenderTest");
	auto file = folder.path / "sub" / "server_warnings.log";
	{
		FileAppender appender(file, std::make_unique<PatternLayout>("%level %logger - %msg%n"), LevelFilter::exactly(spdlog::level::warn));
		EXPECT_EQ(appender.getFile(), file);
		log(appender, "info", spdlog::level::info);
		log(appender, "warning 1", spdlog::level::warn);
		log(appender, "error", spdlog::level::err);
		EXPECT_EQ(readFile(file), "WARN test.Logger - warning 1" + EOL); // flushed immediately, readable while open
	}
	{
		FileAppender appender(file, std::make_unique<PatternLayout>("%msg%n"));
		log(appender, "appended", spdlog::level::trace);
	}
	EXPECT_EQ(readFile(file), "WARN test.Logger - warning 1" + EOL + "appended" + EOL);
}

TEST(FileAppenderTest, DelayedFlush) {
	TempFolder folder("aion_FileAppenderTest_flush");
	auto file = folder.path / "item.log";
	FileAppender appender(file, std::make_unique<PatternLayout>("%msg%n"), std::nullopt, false);
	log(appender, "buffered", spdlog::level::info);
	EXPECT_EQ(readFile(file), "");
	appender.flush();
	EXPECT_EQ(readFile(file), "buffered" + EOL);
}

TEST(FileAppenderTest, UnwritableFileThrows) {
	TempFolder folder("aion_FileAppenderTest_invalid");
	std::filesystem::create_directories(folder.path / "directory.log");
	EXPECT_THROW(FileAppender(folder.path / "directory.log", std::make_unique<PatternLayout>("%msg")), utils::IOException);
}

TEST(FileAppenderTest, OpenFileCanBeDeletedAndRenamed) {
	// Java opens files with FILE_SHARE_DELETE on Windows, so e.g. Logging::archiveLogs can move files that an appender still writes to
	TempFolder folder("aion_FileAppenderTest_share");
	auto file = folder.path / "chat.log";
	FileAppender appender(file, std::make_unique<PatternLayout>("%msg%n"));
	log(appender, "first", spdlog::level::info);
	std::filesystem::rename(file, folder.path / "renamed.log");
	EXPECT_EQ(readFile(folder.path / "renamed.log"), "first" + EOL);
	log(appender, "second", spdlog::level::info);
	std::filesystem::remove(folder.path / "renamed.log");
	EXPECT_FALSE(std::filesystem::exists(folder.path / "renamed.log"));
	log(appender, "into the deleted file", spdlog::level::info);
}

// logback RollingFileAppender with FixedWindowRollingPolicy and SizeBasedTriggeringPolicy (upstream 36bdccb4d)
TEST(FileAppenderTest, RollsOverAtTheSizeLimitAndKeepsTheWindow) {
	TempFolder folder("aion_FileAppenderTest_rolling");
	auto file = folder.path / "server_console.log";
	FileAppender::RollingPolicy policy{.fileNamePattern = folder.path / "server_console.%i.log", .minIndex = 1, .maxIndex = 2, .maxFileSize = 10};
	EXPECT_EQ(policy.fileNameOf(3), folder.path / "server_console.3.log");
	{
		FileAppender appender(file, std::make_unique<PatternLayout>("%msg%n"), std::nullopt, true, policy);
		log(appender, "message-1", spdlog::level::info); // 9 bytes + EOL: reaches the limit, the next message rolls over first
		EXPECT_FALSE(std::filesystem::exists(folder.path / "server_console.1.log"));
		log(appender, "message-2", spdlog::level::info);
		EXPECT_EQ(readFile(folder.path / "server_console.1.log"), "message-1" + EOL);
		EXPECT_EQ(readFile(file), "message-2" + EOL);
		log(appender, "message-3", spdlog::level::info);
		log(appender, "message-4", spdlog::level::info); // the window holds two files: message-1 is dropped
		EXPECT_EQ(readFile(file), "message-4" + EOL);
		EXPECT_EQ(readFile(folder.path / "server_console.1.log"), "message-3" + EOL);
		EXPECT_EQ(readFile(folder.path / "server_console.2.log"), "message-2" + EOL);
		EXPECT_FALSE(std::filesystem::exists(folder.path / "server_console.3.log"));
	}
	{
		// an existing file counts with its size
		FileAppender appender(file, std::make_unique<PatternLayout>("%msg%n"), std::nullopt, true, policy);
		log(appender, "message-5", spdlog::level::info);
		EXPECT_EQ(readFile(file), "message-5" + EOL);
		EXPECT_EQ(readFile(folder.path / "server_console.1.log"), "message-4" + EOL);
	}
}

TEST(FileAppenderTest, RollingWindowIsBoundedLikeLogback) {
	TempFolder folder("aion_FileAppenderTest_window");
	FileAppender below(folder.path / "a.log", std::make_unique<PatternLayout>("%msg%n"), std::nullopt, true,
		FileAppender::RollingPolicy{.fileNamePattern = folder.path / "a.%i.log", .minIndex = 1, .maxIndex = 0});
	EXPECT_EQ(below.getRollingPolicy()->maxIndex, 1);
	FileAppender above(folder.path / "b.log", std::make_unique<PatternLayout>("%msg%n"), std::nullopt, true,
		FileAppender::RollingPolicy{.fileNamePattern = folder.path / "b.%i.log", .minIndex = 1, .maxIndex = 50});
	EXPECT_EQ(above.getRollingPolicy()->maxIndex, 21);
}
