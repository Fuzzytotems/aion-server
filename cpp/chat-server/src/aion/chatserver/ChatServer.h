#pragma once

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "aion/commons/configuration/Properties.h"
#include "aion/commons/database/DatabaseFactory.h"

/**
 * Startup and shutdown of the chat server.
 * <p>
 * main() runs the Java startup sequence (startup()), installs the shutdown hook (Ctrl+C, Ctrl+Break and closing the console window on Windows;
 * SIGINT and SIGTERM elsewhere), waits until the hook fires (or requestShutdown() is called, or the stop file appears) and then runs the Java
 * shutdown hook sequence (shutdown()).
 * <p>
 * Java: com.aionemu.chatserver.ChatServer
 *
 * @author ATracer, KID, nrg
 */
namespace aion::chatserver::ChatServer {

/** C++ addition: the parsed command line */
struct Arguments {
	/** -D&lt;key&gt;=&lt;value&gt; arguments */
	commons::configuration::Properties overrides;
	/** --stop-file=&lt;path&gt; */
	std::optional<std::filesystem::path> stopFile;
	/** arguments of neither form */
	std::vector<std::string> unknownArguments;
};

/**
 * Runs the chat server until it is asked to shut down.
 * <p>
 * C++ additions (command line arguments):
 * <ul>
 * <li><tt>-D&lt;key&gt;=&lt;value&gt;</tt> overrides a configuration property, like an entry in config/mycs.properties (e.g.
 * <tt>-Dchatserver.network.gameserver.password=secret</tt>)</li>
 * <li><tt>--stop-file=&lt;path&gt;</tt>: the server polls the file every 200 ms; when it exists, the file is deleted and the server shuts down like
 * on Ctrl+C (used by tests and scripts that cannot send console events; the game server has the same option)</li>
 * </ul>
 * Other arguments are ignored with a warning.
 * <p>
 * Deviation: if the startup fails, the error is logged ("Critical Error - Thread [main] terminated abnormally"), the components started so far
 * are shut down and ExitCode::ERROR_ is returned (Java: the main thread dies, and threads started before keep the JVM alive).
 *
 * @param args the command line arguments without the program name
 * @return ExitCode::NORMAL after a regular shutdown, ExitCode::ERROR_ if the startup failed
 */
int main(std::span<const std::string_view> args);

/**
 * Java: ChatServer.main - Logging::init (with the CHAT_LOG logger of logback.xml), UncaughtExceptionHandler::install, Config::load (with the
 * overrides), DatabaseFactory::init (with databaseOptions()), IdFactory, GameServerService, ChatService, BroadcastService,
 * VersionInfo/SystemInfo, NettyServer.
 *
 * @throws std::exception if a step fails
 */
void startup(const commons::configuration::Properties& overrides);

/**
 * C++ addition: the chat server's database requirements. Pooled connections get a socket timeout of 60 seconds unless database.socket_timeout
 * or the socketTimeout parameter of database.url sets one (0 = none): a chat log insert on a database that does not answer ends, so the event
 * thread running it (and the shutdown) is not blocked forever. Java: the JDBC driver's default, no timeout.
 */
commons::database::DatabaseFactory::Options databaseOptions();

/** Java: ShutdownHook.run - NettyServer::shutdownAll, then Logging::shutdown (flushes all pending log messages). */
void shutdown();

/** Wakes up main() to shut the server down (what the console control handler / signal handler does). Thread safe, may be called at any time. */
void requestShutdown() noexcept;

/** Parses the command line arguments (see main). */
Arguments parseArguments(std::span<const std::string_view> args);

} // namespace aion::chatserver::ChatServer
