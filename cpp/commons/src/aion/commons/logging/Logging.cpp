#include "aion/commons/logging/Logging.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <mutex>
#include <set>
#include <stacktrace>
#include <thread>
#include <vector>

#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/stdout_sinks.h>

#include "aion/commons/logging/ConsoleAppender.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/logging/PatternLayout.h"
#include "aion/commons/utils/DateTimeFormatter.h"
#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/commons/utils/ZipWriter.h"
#include "aion/commons/utils/info/SystemInfo.h"

namespace aion::commons::logging::Logging {

namespace {

namespace fs = std::filesystem;

struct State {
	std::mutex mutex;
	fs::path logFolder = "log";
	const std::chrono::time_zone* timeZone = nullptr;
	std::vector<std::shared_ptr<DiscordChannelAppender>> discordAppenders;
	/** file appenders created by createFileAppender, flushed by shutdown */
	std::vector<std::weak_ptr<FileAppender>> fileAppenders;
	bool initialized = false;
	bool shutdownRegistered = false;
	/** loads the debug symbols for stack traces in the background, see init */
	std::thread symbolLoader;
	bool symbolsLoading = false;
};

State& state() {
	// Deliberately leaked, like the LoggerFactory registry: it must stay valid during static destruction
	static State* instance = new State();
	return *instance;
}

void shutdownAtExit() {
	try {
		shutdown();
	} catch (...) {
	}
}

std::chrono::system_clock::time_point toSystemTime(fs::file_time_type time) {
	return std::chrono::time_point_cast<std::chrono::system_clock::duration>(std::chrono::clock_cast<std::chrono::system_clock>(time));
}

std::string toUtf8(const fs::path& path) {
	std::u8string text = path.generic_u8string();
	return std::string(text.begin(), text.end());
}

void createArchive(const fs::path& file, const std::vector<fs::path>& files, const fs::path& rootDirectory) {
	fs::create_directories(file.parent_path());
	utils::ZipWriter zip(file);
	for (const fs::path& logFile : files)
		zip.addFile(toUtf8(logFile.lexically_relative(rootDirectory)), logFile);
	zip.finish();
}

void remove(const std::vector<fs::path>& files) {
	for (const fs::path& logFile : files)
		fs::remove(logFile);
	// attempt to delete parent folders (only deletes empty folders)
	std::set<fs::path> parents;
	for (const fs::path& logFile : files)
		parents.insert(logFile.parent_path());
	for (const fs::path& parent : parents) {
		std::error_code ignored;
		fs::remove(parent, ignored);
	}
}

} // namespace

void archiveLogs(const fs::path& logFolder) {
	try {
		fs::path oldLogsFolder = logFolder / "archived";
		fs::path startTimeFile = logFolder / "[server_start_marker]";
		std::vector<fs::path> logFiles;
		std::optional<std::chrono::system_clock::time_point> lastStartTime;
		std::optional<fs::file_time_type> lastStopTime;

		fs::create_directories(logFolder);
		if (fs::exists(startTimeFile)) {
			lastStartTime = toSystemTime(fs::last_write_time(startTimeFile));
		} else if (!std::ofstream(startTimeFile)) {
			throw utils::IOException("Could not create " + startTimeFile.string());
		}
		auto startTime = std::chrono::floor<std::chrono::milliseconds>(utils::info::SystemInfo::getProcessStartTime());
		fs::last_write_time(startTimeFile, std::chrono::clock_cast<fs::file_time_type::clock>(startTime)); // update with new start time

		for (const fs::directory_entry& entry : fs::recursive_directory_iterator(logFolder)) {
			if (entry.is_regular_file() && utils::StringUtils::toLowerCase(toUtf8(entry.path())).ends_with(".log")) {
				logFiles.push_back(entry.path());
				fs::file_time_type lastModified = entry.last_write_time();
				if (!lastStopTime || *lastStopTime < lastModified)
					lastStopTime = lastModified;
			}
		}

		if (!logFiles.empty()) {
			utils::DateTimeFormatter dtf = utils::DateTimeFormatter::ofPattern("yyyy-MM-dd HH.mm"); // system default time zone
			std::string outFilename = (lastStartTime ? dtf.format(*lastStartTime) : "Unknown") + " to " + dtf.format(toSystemTime(*lastStopTime)) + ".zip";
			createArchive(oldLogsFolder / fs::path(std::u8string(outFilename.begin(), outFilename.end())), logFiles, logFolder);
			remove(logFiles);
		}
	} catch (const std::exception&) {
		throw utils::Exception("Error gathering and archiving old logs", std::current_exception());
	}
}

std::vector<std::string> getPropertyKeys(std::string_view serverName) {
	std::string prefix(serverName);
	return {prefix + ".log.status.discord.webhook_url", prefix + ".log.status.discord.avatar_url"};
}

std::string resolvePattern(std::string_view pattern) {
	std::string resolved = utils::StringUtils::replace(pattern, "${date}", DATE_PATTERN);
	return utils::StringUtils::replace(resolved, "${consoleTime}", CONSOLE_TIME_PATTERN);
}

std::shared_ptr<FileAppender> createFileAppender(const fs::path& file, std::string_view pattern, std::optional<LevelFilter> filter, bool immediateFlush) {
	State& s = state();
	std::lock_guard lock(s.mutex);
	fs::path path = file.is_relative() ? s.logFolder / file : file;
	auto appender = std::make_shared<FileAppender>(path, std::make_unique<PatternLayout>(resolvePattern(pattern), s.timeZone), filter, immediateFlush);
	std::erase_if(s.fileAppenders, [](const auto& weak) { return weak.expired(); });
	s.fileAppenders.push_back(appender);
	return appender;
}

std::shared_ptr<DiscordChannelAppender> createDiscordAppender(std::string name, std::string webhookUrl, std::string_view pattern,
	std::optional<LevelFilter> filter) {
	if (webhookUrl.empty())
		return nullptr;
	State& s = state();
	std::lock_guard lock(s.mutex);
	DiscordChannelAppender::Config config{
		.name = std::move(name),
		.webhookUrl = std::move(webhookUrl),
		.encoder = std::make_unique<PatternLayout>(resolvePattern(pattern), s.timeZone),
		.userNameAvatarUrlMessageSeparator = "|",
		.filter = filter,
	};
	auto appender = std::make_shared<DiscordChannelAppender>(std::move(config));
	s.discordAppenders.push_back(appender);
	return appender;
}

void init(const Config& config) {
	State& s = state();
	bool reinitialize;
	{
		std::lock_guard lock(s.mutex);
		reinitialize = s.initialized;
	}
	if (reinitialize)
		shutdown(); // closes the log files, so they can be archived

	{
		// The first symbolization of a stack trace loads the debug symbols (about a second for a large program) under a process-wide lock, which
		// would stall the first thread that logs an exception (e.g. an IO thread) and all threads logging exceptions meanwhile. Load them now.
		std::lock_guard lock(s.mutex);
		if (!s.symbolsLoading) {
			s.symbolsLoading = true;
			try {
				s.symbolLoader = std::thread([] {
					try {
						[[maybe_unused]] std::string trace = std::to_string(std::stacktrace::current());
					} catch (...) {
					}
				});
			} catch (const std::exception&) {
			}
		}
	}

	if (config.archiveLogs)
		archiveLogs(config.logFolder);
	{
		std::lock_guard lock(s.mutex);
		s.logFolder = config.logFolder;
		s.timeZone = config.timeZone;
		s.initialized = true;
	}

	std::vector<spdlog::sink_ptr> sinks;
	auto reportError = [](std::string_view appender, const std::exception& e) {
		// logback reports appender errors on the console and continues without them
		std::fprintf(stderr, "ERROR in %.*s - %s\n", static_cast<int>(appender.size()), appender.data(), e.what());
	};
	if (config.console) {
		try {
			sinks.push_back(std::make_shared<ConsoleAppender>(resolvePattern(CONSOLE_PATTERN), config.timeZone));
		} catch (const std::exception& e) {
			reportError("out_console", e);
		}
	}
	auto addFileAppender = [&](std::string_view appender, const char* file, std::string_view pattern, std::optional<LevelFilter> filter) {
		try {
			sinks.push_back(createFileAppender(file, pattern, filter));
		} catch (const std::exception& e) {
			reportError(appender, e);
		}
	};
	addFileAppender("app_console", "server_console.log", SERVER_CONSOLE_FILE_PATTERN, std::nullopt);
	addFileAppender("app_error", "server_errors.log", WARNINGS_AND_ERRORS_FILE_PATTERN, LevelFilter::exactly(spdlog::level::err));
	addFileAppender("app_warn", "server_warnings.log", WARNINGS_AND_ERRORS_FILE_PATTERN, LevelFilter::exactly(spdlog::level::warn));
	try {
		std::string statusPattern = utils::StringUtils::replace(STATUS_DISCORD_PATTERN, "${avatarUrl}", config.statusDiscordAvatarUrl);
		if (auto discord = createDiscordAppender("app_status_discord_async", config.statusDiscordWebhookUrl, statusPattern,
					LevelFilter::threshold(spdlog::level::warn)))
			sinks.push_back(std::move(discord));
	} catch (const std::exception& e) {
		reportError("app_status_discord", e);
	}

	std::vector<spdlog::sink_ptr> previousSinks = LoggerFactory::rootSink()->sinks(); // destroyed after the root sink's lock is released
	LoggerFactory::rootSink()->set_sinks(std::move(sinks));
	LoggerFactory::setRootLevel(config.rootLevel);

	// Java: LoggerContext.stop() runs in a shutdown hook, also on System.exit. Registered after everything the appenders use was initialized, so
	// it runs before those objects are destroyed.
	std::lock_guard lock(s.mutex);
	if (!s.shutdownRegistered) {
		s.shutdownRegistered = true;
		std::atexit(&shutdownAtExit);
	}
}

void shutdown() {
	std::vector<std::shared_ptr<DiscordChannelAppender>> discordAppenders;
	std::vector<std::shared_ptr<FileAppender>> fileAppenders;
	std::thread symbolLoader;
	{
		State& s = state();
		std::lock_guard lock(s.mutex);
		symbolLoader.swap(s.symbolLoader);
		discordAppenders.swap(s.discordAppenders);
		for (const auto& weak : s.fileAppenders) {
			if (auto appender = weak.lock())
				fileAppenders.push_back(std::move(appender));
		}
		s.fileAppenders.clear();
		s.initialized = false;
	}
	LoggerFactory::rootSink()->flush();
	for (auto& appender : fileAppenders)
		appender->flush();
	for (auto& appender : discordAppenders)
		appender->stop(); // messages logged meanwhile still reach the other appenders
	if (symbolLoader.joinable())
		symbolLoader.join(); // must not run while the process exits
	std::vector<spdlog::sink_ptr> previousSinks = LoggerFactory::rootSink()->sinks(); // destroyed after the root sink's lock is released
	LoggerFactory::rootSink()->set_sinks({std::make_shared<spdlog::sinks::stderr_sink_mt>()});
}

std::unique_ptr<spdlog::formatter> createSpdlogFormatter(const std::string& pattern) {
	auto formatter = std::make_unique<spdlog::pattern_formatter>();
	formatter->add_flag<ThreadNameFlagFormatter>('*').set_pattern(pattern);
	return formatter;
}

} // namespace aion::commons::logging::Logging
