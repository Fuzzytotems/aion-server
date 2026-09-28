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
