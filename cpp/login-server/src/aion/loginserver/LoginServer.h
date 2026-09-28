#pragma once

#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "aion/commons/configuration/Properties.h"

/**
 * Startup and shutdown of the login server.
 * <p>
 * main() runs the Java startup sequence (startup()), installs the shutdown hook (Ctrl+C, Ctrl+Break and closing the console window on Windows;
 * SIGINT and SIGTERM elsewhere), waits until the hook fires (or requestShutdown() is called) and then runs the Java shutdown hook sequence
 * (shutdown()).
 * <p>
 * Java: com.aionemu.loginserver.LoginServer
 *
 * @author -Nemesiss-
 */
namespace aion::loginserver::LoginServer {

/**
 * Runs the login server until it is asked to shut down.
 * <p>
 * C++ addition: arguments of the form <tt>-D&lt;key&gt;=&lt;value&gt;</tt> override configuration properties after Config::load() (for example
 * <tt>-Ddatabase.url=jdbc:mysql://localhost:3306/aion_ls_run</tt>), like entries in config/myls.properties. Other arguments are ignored with a
 * warning.
 * <p>
 * Deviation: if the startup fails, the error is logged ("Critical Error - Thread [main] terminated abnormally"), the components started so far
 * are shut down and ExitCode::ERROR_ is returned (Java: the main thread dies, while threads started before, e.g. PlayerTransferService's, keep the
 * JVM alive).
 *
 * @param args the command line arguments without the program name
 * @return ExitCode::NORMAL after a regular shutdown, ExitCode::ERROR_ if the startup failed
 */
int main(std::span<const std::string_view> args);

/**
 * Java: LoginServer.main - Logging::init (with the CM_LOGIN audit log), UncaughtExceptionHandler::install, Config::load (plus overrides),
 * DatabaseFactory::init, KeyGen::init, GameServerTable::load, BannedIpController::start, BannedMacDAO/BannedHddDAO::cleanExpiredBans,
 * PlayerTransferService, VersionInfo/SystemInfo, NetConnector::connect.
 *
 * @param overrides properties that override the loaded configuration (may be empty)
 * @throws std::exception if a step fails
 */
void startup(const commons::configuration::Properties& overrides);

/**
 * Java: ShutdownHook.run - PlayerTransferService shutdown, NetConnector shutdown and Logging shutdown (flushes all pending log messages).
 */
void shutdown();

/** Wakes up main() to shut the server down (what the console control handler / signal handler does). Thread safe, may be called at any time. */
void requestShutdown() noexcept;

/**
 * Parses <tt>-Dkey=value</tt> arguments.
 *
 * @param unknownArguments receives the arguments that are not of that form
 */
commons::configuration::Properties parseOverrides(std::span<const std::string_view> args, std::vector<std::string>& unknownArguments);

} // namespace aion::loginserver::LoginServer
