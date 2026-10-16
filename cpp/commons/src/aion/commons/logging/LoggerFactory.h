#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <spdlog/sinks/dist_sink.h>

#include "aion/commons/logging/Logger.h"

namespace aion::commons::logging {

/**
 * Per-logger configuration, equivalent to a logback &lt;logger name="..." level="..." additivity="..."&gt; element. It applies to the named
 * logger and all loggers below it in the dot-separated hierarchy (e.g. "com.aionemu.loginserver" is an ancestor of
 * "com.aionemu.loginserver.dao.AccountDAO"), with logback's rules:
 * <ul>
 * <li>the effective level of a logger is the level of the nearest configured logger (itself or an ancestor) that sets one, otherwise the root
 * level</li>
 * <li>a message is written to the sinks of the logger's own config and of all configured ancestors, up to and including the first one with
 * additive = false; if all of them are additive, also to the root sink</li>
 * </ul>
 * An empty config (no level, no sinks, additive) therefore has no effect.
 */
struct LoggerConfig {
	/** Minimum level, or nullopt to inherit the level of the nearest ancestor that has one (or the root level). */
	std::optional<spdlog::level::level_enum> level;
	/** Additional sinks (logback appender-refs) for these loggers. */
	std::vector<spdlog::sink_ptr> sinks;
	/** If false, messages are not passed to the sinks of ancestors and the root sinks (logback additivity="false"). */
	bool additive = true;
};

/**
 * Java: org.slf4j.LoggerFactory. Creates named loggers that write to the root sink, whose actual outputs (console, files, Discord, ...) are
 * set up by Logging::init. Loggers may be created before Logging::init (e.g. as static variables); until then they log to stderr.
 * <p>
 * By convention, logger names are the fully qualified Java class names of the classes being ported, e.g.
 * "com.aionemu.loginserver.dao.AccountDAO", so log output and configuration match the Java server.
 */
namespace LoggerFactory {

Logger getLogger(std::string_view name);

/**
 * The distributing sink all additive loggers write to (logback: appenders referenced by &lt;root&gt;). Logging::init replaces its sinks.
 * Its set_sinks/remove_sink destroy removed sinks while holding the sink's lock, so stop asynchronous appenders (DiscordChannelAppender::stop)
 * before removing them.
 */
std::shared_ptr<spdlog::sinks::dist_sink_mt> rootSink();

/** Sets the level of the root logger (default: info). Updates existing loggers. */
void setRootLevel(spdlog::level::level_enum level);

/**
 * Configures a logger subtree (replacing an earlier config of the same name). Existing loggers are updated immediately.
 * <p>
 * Thread safety: all functions of LoggerFactory may be called while other threads log. A message being logged concurrently is written either
 * with the old or with the new configuration. Sinks that are no longer referenced are destroyed after the internal locks are released.
 */
void configure(std::string_view name, LoggerConfig config);

/** Removes the config of the given name (if any), so the loggers below it use the configs of their ancestors again. */
void removeConfig(std::string_view name);

/** Flushes all sinks. Call before exiting (Java: LoggerContext.stop()). */
void flushAll();

} // namespace LoggerFactory

} // namespace aion::commons::logging
