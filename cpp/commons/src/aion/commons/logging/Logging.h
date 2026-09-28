#pragma once

#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <spdlog/common.h>
#include <spdlog/formatter.h>

#include "aion/commons/logging/DiscordChannelAppender.h"
#include "aion/commons/logging/FileAppender.h"
#include "aion/commons/logging/LevelFilter.h"

/**
 * Java: com.aionemu.commons.logging.Logging plus the root logger configuration of the servers' config/logback.xml, which is identical for the
 * login, chat and game server:
 * <table>
 * <tr><th>logback appender</th><th>output</th></tr>
 * <tr><td>out_console</td><td>console: <tt>HH:mm:ss LEVEL [thread] - message</tt>, level highlighted, thread gray</td></tr>
 * <tr><td>app_console</td><td>log/server_console.log: all messages</td></tr>
 * <tr><td>app_error</td><td>log/server_errors.log: ERROR messages only</td></tr>
 * <tr><td>app_warn</td><td>log/server_warnings.log: WARN messages only</td></tr>
 * <tr><td>app_status_discord_async</td><td>Discord webhook: WARN and above, if a webhook URL is configured</td></tr>
 * </table>
 * Server ports add their extra appenders for specific loggers (the logback &lt;logger&gt; elements) with createFileAppender and
 * LoggerFactory::configure, e.g. for the game server's <tt>&lt;logger name="ITEM_LOG" additivity="false"&gt;</tt>:
 * <pre>
 * LoggerFactory::configure("ITEM_LOG", {.sinks = {Logging::createFileAppender("item.log", "${date} %message%n", std::nullopt, false)}, .additive = false});
 * </pre>
 * Since logback.xml is not read, the properties it takes from the config files (Discord webhook and avatar URL, gameserver.timezone) must be read by
 * the server before calling init.
 */
namespace aion::commons::logging::Logging {

/** logback.xml property "date" */
inline constexpr std::string_view DATE_PATTERN = "%date{\"yyyy-MM-dd'T'HH:mm:ss,SSSXXX\"}";
/** logback.xml property "consoleTime" */
inline constexpr std::string_view CONSOLE_TIME_PATTERN = "%date{HH:mm:ss}";
/** out_console */
inline constexpr std::string_view CONSOLE_PATTERN = "${consoleTime} %highlight(%-5level) %gray([%thread]) - %message%n";
/** app_console */
inline constexpr std::string_view SERVER_CONSOLE_FILE_PATTERN = "${date} %-5level [%thread] %logger - %message%n";
/** app_warn and app_error */
inline constexpr std::string_view WARNINGS_AND_ERRORS_FILE_PATTERN = "${date} %logger - %message%n";
/** app_status_discord, ${avatarUrl} is replaced by the configured avatar URL */
inline constexpr std::string_view STATUS_DISCORD_PATTERN =
	"%logger{0} [%thread]|${avatarUrl}|%msg%replace(%n```qml%n%ex```){\\r?\\n```qml\\r?\\n```, ''}%nopex";

struct Config {
	/** logback.xml property "logFolder" */
	std::filesystem::path logFolder = "log";
	/** time zone of the logged dates (game server: gameserver.timezone), nullptr for the system default time zone */
	const std::chrono::time_zone* timeZone = nullptr;
	/** &lt;server&gt;.log.status.discord.webhook_url, empty to disable the Discord appender */
	std::string statusDiscordWebhookUrl;
	/**
	 * &lt;server&gt;.log.status.discord.avatar_url. It is inserted into the conversion pattern and can therefore use conversion words such as
	 * %level.
	 */
	std::string statusDiscordAvatarUrl;
	/** root level (logback: &lt;root level="INFO"&gt;) */
	spdlog::level::level_enum rootLevel = spdlog::level::info;
	/** Java: Logging.init archives the log files of the previous run */
	bool archiveLogs = true;
	/** write to the console (out_console) */
	bool console = true;
};

/**
 * Java: Logging.init() and the configuration of the root logger by logback.xml. Archives the old log files (see archiveLogs), then replaces
 * the root sinks of LoggerFactory with the appenders listed above and sets the root level. Loggers created before keep working and use the
 * new appenders.
 * <p>
 * Log files that cannot be opened are reported on stderr and skipped, like logback does.
 *
 * @throws Exception if archiving the old log files fails
 */
void init(const Config& config = {});

/**
 * Java: LoggerContext.stop() in the shutdown hooks - flushes the appenders created by init and createFileAppender, sends the queued Discord
 * messages of the appenders created by init and createDiscordAppender (waiting up to one second each) and closes the root appenders. Afterwards
 * the root logger writes to stderr again. Sinks created elsewhere can be flushed with LoggerFactory::flushAll().
 * <p>
 * init registers this function with std::atexit, so it also runs when the process exits normally without calling it (returning from main or
 * std::exit), before the objects the appenders use are destroyed.
 */
void shutdown();

/**
 * Moves the *.log files of the log folder (including subfolders) of the previous run into a ZIP archive
 * "&lt;log folder&gt;/archived/&lt;last start&gt; to &lt;last modification&gt;.zip" (dates as "yyyy-MM-dd HH.mm", or "Unknown" if the
 * start marker file "[server_start_marker]" did not exist) and updates the start marker to the start time of this process.
 *
 * @throws Exception "Error gathering and archiving old logs" with the cause
 */
void archiveLogs(const std::filesystem::path& logFolder);

/**
 * Java: the ${...} references to server properties in config/logback.xml that are read by Logging's configuration (Config), which servers exclude
 * from the "unknown property" warnings (Config.removePropertiesUsedInLogbackXml). The server's own logging properties (e.g. the chat server's
 * chatserver.log.chat.discord.*) and gameserver.timezone, which is also a config field, are not included.
 *
 * @param serverName the property prefix of the server: "loginserver", "chatserver" or "gameserver"
 * @return "&lt;serverName&gt;.log.status.discord.webhook_url" and "&lt;serverName&gt;.log.status.discord.avatar_url"
 */
std::vector<std::string> getPropertyKeys(std::string_view serverName);

/** @return the pattern with the logback.xml properties ${date} and ${consoleTime} replaced */
std::string resolvePattern(std::string_view pattern);

/**
 * Java: a logback FileAppender with PatternLayoutEncoder. The time zone is the one passed to init.
 *
 * @param file the file, relative paths are resolved against the log folder of init (Java: ${logFolder}/file)
 * @param pattern logback conversion pattern; ${date} and ${consoleTime} are resolved
 * @param filter optional level filter
 * @param immediateFlush flush after each message (logback default: true)
 * @throws IOException if the file cannot be opened
 */
std::shared_ptr<FileAppender> createFileAppender(const std::filesystem::path& file, std::string_view pattern,
	std::optional<LevelFilter> filter = std::nullopt, bool immediateFlush = true);

/**
 * Java: a DiscordChannelAppender with userName_avatarUrl_msg_separator "\|" wrapped in an AsyncAppender with neverBlock=true. It is stopped by
 * shutdown().
 *
 * @param name appender name (for the worker thread name)
 * @param webhookUrl the webhook URL
 * @param pattern logback conversion pattern (e.g. the chat server's
 *          <tt>%replace(%msg){'\[(.*?) \((.)\)\] (.*?): (.*)', '[$1] $3|avatarUrl|$4'}</tt>)
 * @param filter optional level filter of the AsyncAppender
 * @return the appender, or nullptr if the webhook URL is empty (Java: the appender is not used)
 */
std::shared_ptr<DiscordChannelAppender> createDiscordAppender(std::string name, std::string webhookUrl, std::string_view pattern,
	std::optional<LevelFilter> filter = std::nullopt);

/** @return a spdlog pattern formatter (for spdlog pattern syntax) with the custom flag %* for the thread name */
std::unique_ptr<spdlog::formatter> createSpdlogFormatter(const std::string& pattern);

} // namespace aion::commons::logging::Logging
