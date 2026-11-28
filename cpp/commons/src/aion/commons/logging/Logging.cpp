#include "aion/commons/logging/Logging.h"

#include <cstdio>
#include <cstdlib>
#include <format>
#include <fstream>
#include <functional>
#include <mutex>
#include <regex>
#include <set>
#include <stacktrace>
#include <thread>
#include <vector>

#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/stdout_sinks.h>

#include "aion/commons/Banner.h"
#include "aion/commons/logging/ConsoleAppender.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/logging/PatternLayout.h"
#include "aion/commons/utils/DateTimeFormatter.h"
#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/commons/utils/ZipWriter.h"
#include "aion/commons/utils/concurrent/ThreadName.h"
#include "aion/commons/utils/concurrent/UncaughtExceptionHandler.h"
#include "aion/commons/utils/info/SystemInfo.h"
#include "aion/commons/utils/info/VersionInfo.h"

namespace aion::commons::logging::Logging {

namespace {

namespace fs = std::filesystem;

struct State {
	std::mutex mutex;
	fs::path logFolder = "log";
	const std::chrono::time_zone* timeZone = nullptr;
	uint64_t maxFileSize = Config{}.maxFileSize;
	int32_t maxRolledFiles = Config{}.maxRolledFiles;
	std::vector<std::shared_ptr<DiscordChannelAppender>> discordAppenders;
	/** file appenders created by createFileAppender, flushed by shutdown */
	std::vector<std::weak_ptr<FileAppender>> fileAppenders;
	bool initialized = false;
	bool shutdownRegistered = false;
	/** loads the debug symbols for stack traces in the background, see init */
	std::thread symbolLoader;
	/** Java: the "Log-Archiver" thread of archiveLogs */
	std::thread logArchiver;
	bool symbolsLoading = false;
};

State& state() {
	// Deliberately leaked, like the LoggerFactory registry: it must stay valid during static destruction
	static State* instance = new State();
	return *instance;
}

/** @return the environment variable, or nothing if it is not set */
std::optional<std::string> environmentVariable(const std::string& name) {
#ifdef _WIN32
	char* value = nullptr;
	size_t length = 0;
	if (_dupenv_s(&value, &length, name.c_str()) != 0 || value == nullptr)
		return std::nullopt;
	std::string result(value);
	std::free(value);
	return result;
#else
	const char* value = std::getenv(name.c_str());
	return value ? std::optional<std::string>(value) : std::nullopt;
#endif
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
		zip.addFile(toUtf8(logFile.lexically_relative(rootDirectory)), logFile); // the entry gets the file's modification time
	zip.finish();
}

void remove(const std::vector<fs::path>& files) {
	std::set<fs::path, std::greater<>> parents; // Java: TreeSet<>(Comparator.reverseOrder()), the deepest folders first
	for (const fs::path& logFile : files) {
		std::error_code ignored;
		fs::remove(logFile, ignored); // Java: Files.deleteIfExists (C++: an error is ignored like a missing file)
		parents.insert(logFile.parent_path());
	}
	for (const fs::path& parent : parents) {
		std::error_code notEmpty;
		fs::remove(parent, notEmpty); // only deletes empty folders
	}
}

/** Java: moveOldLogFiles - moves the *.log files (outside the archive folder) into the staging folder and removes the folders they leave empty */
std::vector<fs::path> moveOldLogFiles(const fs::path& logFolder, const fs::path& oldLogsFolder, const fs::path& stagingFolder,
	std::optional<fs::file_time_type>& lastStopTime) {
	if (!fs::exists(logFolder))
		return {};
	std::vector<fs::path> logFiles;
	std::vector<fs::path> directories;
	for (auto it = fs::recursive_directory_iterator(logFolder); it != fs::recursive_directory_iterator(); ++it) {
		const fs::directory_entry& entry = *it;
		if (entry.is_directory()) {
			// Skip the archived/ folder to avoid re-processing leftover subdirectories
			if (entry.path() == oldLogsFolder) {
				it.disable_recursion_pending();
				continue;
			}
			directories.push_back(entry.path());
		} else if (entry.is_regular_file() && utils::StringUtils::toLowerCase(toUtf8(entry.path())).ends_with(".log")) {
			fs::file_time_type lastModified = entry.last_write_time();
			fs::path target = stagingFolder / entry.path().lexically_relative(logFolder);
			fs::create_directories(target.parent_path());
			fs::rename(entry.path(), target); // Java: Files.move(file, target, REPLACE_EXISTING)
			logFiles.push_back(target);
			if (!lastStopTime || *lastStopTime < lastModified)
				lastStopTime = lastModified;
		}
	}
	// Java: postVisitDirectory deletes each visited folder but the log folder, if it is empty now (the deepest first)
	for (auto dir = directories.rbegin(); dir != directories.rend(); ++dir) {
		std::error_code notEmpty;
		fs::remove(*dir, notEmpty);
	}
	return logFiles;
}

/** Java: updateStartTime */
void updateStartTime(const fs::path& startTimeFile) {
	if (!fs::exists(startTimeFile)) {
		fs::create_directories(startTimeFile.parent_path());
		if (!std::ofstream(startTimeFile))
			throw utils::IOException("Could not create " + startTimeFile.string());
	}
	auto startTime = std::chrono::floor<std::chrono::milliseconds>(utils::info::SystemInfo::getProcessStartTime());
	fs::last_write_time(startTimeFile, std::chrono::clock_cast<fs::file_time_type::clock>(startTime));
}

} // namespace

void archiveLogs(const fs::path& logFolder) {
	awaitLogArchiver(); // C++: the archiver of an earlier call is done before the files are gathered again
	try {
		fs::path oldLogsFolder = logFolder / "archived";
		fs::path startTimeFile = logFolder / "[server_start_marker]";
		std::optional<std::chrono::system_clock::time_point> lastStartTime;
		if (fs::exists(startTimeFile))
			lastStartTime = toSystemTime(fs::last_write_time(startTimeFile));
		std::optional<fs::file_time_type> lastStopTime;
		utils::DateTimeFormatter dtf = utils::DateTimeFormatter::ofPattern("yyyy-MM-dd HH.mm"); // system default time zone
		std::string stagingName = dtf.format(lastStartTime ? *lastStartTime : std::chrono::system_clock::now());
		fs::path stagingFolder = oldLogsFolder / fs::path(std::u8string(stagingName.begin(), stagingName.end()));

		std::vector<fs::path> logFiles = moveOldLogFiles(logFolder, oldLogsFolder, stagingFolder, lastStopTime);
		// update the start time after old log files have been moved successfully, so that exceptions don't alter the time for the next archival attempt
		updateStartTime(startTimeFile);

		if (!logFiles.empty()) {
			std::string outFilename = (lastStartTime ? dtf.format(*lastStartTime) : "Unknown") + " to " + dtf.format(toSystemTime(*lastStopTime)) + ".zip";
			fs::path archive = oldLogsFolder / fs::path(std::u8string(outFilename.begin(), outFilename.end()));
			// Java: a platform thread "Log-Archiver", so that the JVM completes this task before shutting down (C++: joined by shutdown)
			State& s = state();
			std::lock_guard lock(s.mutex);
			s.logArchiver = std::thread([archive, logFiles = std::move(logFiles), stagingFolder] {
				utils::concurrent::setCurrentThreadName("Log-Archiver");
				try {
					try {
						createArchive(archive, logFiles, stagingFolder);
						remove(logFiles);
					} catch (const std::exception&) {
						throw utils::Exception("Error archiving old logs", std::current_exception());
					}
				} catch (...) {
					utils::concurrent::UncaughtExceptionHandler::uncaughtException("Log-Archiver", std::current_exception());
				}
			});
		}
	} catch (const std::exception&) {
		throw utils::Exception("Error gathering old logs", std::current_exception());
	}
}

void awaitLogArchiver() {
	std::thread archiver;
	{
		State& s = state();
		std::lock_guard lock(s.mutex);
		archiver.swap(s.logArchiver);
	}
	if (archiver.joinable())
		archiver.join();
}

std::vector<std::string> getPropertyKeys(std::string_view serverName) {
	std::string prefix(serverName);
	return {prefix + ".log.status.discord.webhook_url", prefix + ".log.status.discord.avatar_url", prefix + ".log.max_file_size",
		prefix + ".log.max_rolled_files"};
}

uint64_t parseFileSize(std::string_view value) {
	// logback FileSize: Pattern.compile("([0-9]+)\\s*(|kb|mb|gb)s?", Pattern.CASE_INSENSITIVE).matcher(value).matches()
	static const std::regex FILE_SIZE_PATTERN("([0-9]+)\\s*(|kb|mb|gb)s?", std::regex::icase);
	std::string text(value);
	std::smatch match;
	if (std::regex_match(text, match, FILE_SIZE_PATTERN)) {
		std::string unit = utils::StringUtils::toLowerCase(match[2].str());
		uint64_t coefficient = unit == "kb" ? 1024 : unit == "mb" ? 1024 * 1024 : unit == "gb" ? 1024 * 1024 * 1024 : 1;
		try {
			return std::stoull(match[1].str()) * coefficient;
		} catch (const std::exception&) {
		}
	}
	throw utils::IllegalArgumentException("String value [" + text + "] is not in the expected format.");
}

std::string resolvePattern(std::string_view pattern) {
	std::string resolved = utils::StringUtils::replace(pattern, "${date}", DATE_PATTERN);
	resolved = utils::StringUtils::replace(resolved, "${consoleTime}", CONSOLE_TIME_PATTERN);
	// logback: ${NAME:-default} - the variable, here an environment variable, or the default
	for (size_t start = resolved.find("${"); start != std::string::npos; start = resolved.find("${", start)) {
		size_t separator = resolved.find(":-", start);
		size_t end = resolved.find('}', start);
		if (end == std::string::npos)
			break;
		std::string name = resolved.substr(start + 2, (separator != std::string::npos && separator < end ? separator : end) - start - 2);
		std::string defaultValue = separator != std::string::npos && separator < end ? resolved.substr(separator + 2, end - separator - 2) : "";
		std::optional<std::string> value = environmentVariable(name);
		std::string replacement = value ? *value : defaultValue;
		resolved.replace(start, end - start + 1, replacement);
		start += replacement.size();
	}
	return resolved;
}

void logStartupStats(std::string_view serverClass, std::chrono::steady_clock::time_point startTimeNanos, const utils::info::VersionInfo* versionInfo,
	const std::chrono::time_zone* timeZoneId) {
	using namespace std::chrono;
	auto startedNanos = steady_clock::now();
	auto uptimeMillis = duration_cast<milliseconds>(system_clock::now() - utils::info::SystemInfo::getProcessStartTime());
	std::string_view simpleName = serverClass.substr(serverClass.rfind('.') == std::string_view::npos ? 0 : serverClass.rfind('.') + 1);
	std::string serverName = utils::StringUtils::replace(simpleName, "Server", " server");
	std::string startupMessage = formatStartupMessage(serverName, startTimeNanos, startedNanos, uptimeMillis);
	if (versionInfo == nullptr)
		utils::info::VersionInfo::logAll();
	else
		utils::info::VersionInfo::logAll(*versionInfo, timeZoneId);
	// System.gc(): no counterpart
	utils::info::SystemInfo::logAll();
	LoggerFactory::getLogger(std::string(serverClass)).info(startupMessage);
}

std::string formatStartupMessage(std::string_view server, std::chrono::steady_clock::time_point startTimeNanos,
	std::chrono::steady_clock::time_point startedNanos, std::chrono::milliseconds uptime) {
	using namespace std::chrono;
	auto decimalFormat = [](float seconds) { // DecimalFormat("0.0##", Locale.US): one to three decimals, HALF_EVEN
		std::string text = std::format("{:.3f}", seconds);
		while (text.ends_with('0') && text[text.size() - 2] != '.')
			text.pop_back();
		return text;
	};
	float startTime = static_cast<float>(duration_cast<milliseconds>(startedNanos - startTimeNanos).count()) / 1000.0f;
	return std::format("{} started in {} seconds (process running for {})", server, decimalFormat(startTime),
		decimalFormat(static_cast<float>(uptime.count()) / 1000.0f));
}

fs::path getLogFolder() {
	State& s = state();
	std::lock_guard lock(s.mutex);
	return s.logFolder;
}

std::shared_ptr<FileAppender> createFileAppender(const fs::path& file, std::string_view pattern, std::optional<LevelFilter> filter, bool immediateFlush) {
	State& s = state();
	std::lock_guard lock(s.mutex);
	fs::path path = file.is_relative() ? s.logFolder / file : file;
	// logback.xml: <fileNamePattern>${logFolder}/<name>.%i.log</fileNamePattern> next to <file>${logFolder}/<name>.log</file>
	FileAppender::RollingPolicy rollingPolicy{
		.fileNamePattern = path.parent_path() / fs::path(path.stem().u8string() + u8".%i" + path.extension().u8string()),
		.minIndex = 1,
		.maxIndex = s.maxRolledFiles,
		.maxFileSize = s.maxFileSize,
	};
	auto appender = std::make_shared<FileAppender>(path, std::make_unique<PatternLayout>(resolvePattern(pattern), s.timeZone), filter, immediateFlush,
		rollingPolicy);
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

	// Java: LogLevelHighlightingCompositeConverter.register() and RealGrayCompositeConverter.register() are the colors of PatternLayout
	if (config.console)
		Banner::print();
	if (config.archiveLogs)
		archiveLogs(config.logFolder);
	{
		std::lock_guard lock(s.mutex);
		s.logFolder = config.logFolder;
		s.timeZone = config.timeZone;
		s.maxFileSize = config.maxFileSize;
		s.maxRolledFiles = config.maxRolledFiles;
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
	awaitLogArchiver(); // Java: the JVM waits for the platform thread
	std::vector<spdlog::sink_ptr> previousSinks = LoggerFactory::rootSink()->sinks(); // destroyed after the root sink's lock is released
	LoggerFactory::rootSink()->set_sinks({std::make_shared<spdlog::sinks::stderr_sink_mt>()});
}

std::unique_ptr<spdlog::formatter> createSpdlogFormatter(const std::string& pattern) {
	auto formatter = std::make_unique<spdlog::pattern_formatter>();
	formatter->add_flag<ThreadNameFlagFormatter>('*').set_pattern(pattern);
	return formatter;
}

} // namespace aion::commons::logging::Logging
