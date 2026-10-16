#include "aion/chatserver/ChatServer.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <exception>
#include <memory>
#include <mutex>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#else
#include <csignal>
#endif

#include "aion/chatserver/configs/Config.h"
#include "aion/chatserver/network/netty/NettyServer.h"
#include "aion/chatserver/service/BroadcastService.h"
#include "aion/chatserver/service/ChatService.h"
#include "aion/chatserver/service/GameServerService.h"
#include "aion/chatserver/utils/IdFactory.h"
#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/logging/Logging.h"
#include "aion/commons/utils/ExitCode.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/commons/utils/concurrent/ThreadName.h"
#include "aion/commons/utils/concurrent/UncaughtExceptionHandler.h"
#include "aion/commons/utils/info/SystemInfo.h"
#include "aion/commons/utils/info/VersionInfo.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::chatserver::ChatServer {

using commons::configuration::Properties;
using configs::Config;
namespace Logging = commons::logging::Logging;
namespace LoggerFactory = commons::logging::LoggerFactory;

namespace {

const commons::logging::Logger& log() {
	static const auto* logger = new commons::logging::Logger(LoggerFactory::getLogger("com.aionemu.chatserver.ChatServer"));
	return *logger;
}

/** Java: the poll interval of the game server's stop file (main.cpp), used for the same option here */
constexpr std::chrono::milliseconds STOP_FILE_POLL_INTERVAL{200};

struct ShutdownState {
	std::mutex mutex;
	std::condition_variable changed;
	bool requested = false; // guarded by mutex
	bool done = false; // guarded by mutex
};

ShutdownState& shutdownState() {
	static auto* s = new ShutdownState(); // leaked: the console control handler thread may use it while the process exits
	return *s;
}

/** true once NettyServer was created, so a failed startup does not create it just to shut it down */
std::atomic<bool> nettyServerStarted = false;

/**
 * logback.xml: &lt;logger name="CHAT_LOG" additivity="false"&gt; with the appenders app_chat (log/chat.log, "${date} %message%n") and
 * app_chat_discord_async (a Discord webhook, only if chatserver.log.chat.discord.webhook_url is set).
 */
void configureChatLog() {
	Properties properties = Config::loadLogbackProperties();
	// logback trims the values of properties it defines
	const std::string webhookUrl(commons::utils::StringUtils::trim(properties.getProperty("chatserver.log.chat.discord.webhook_url", "")));
	const std::string avatarUrl(commons::utils::StringUtils::trim(properties.getProperty("chatserver.log.chat.discord.avatar_url", "")));
	commons::logging::LoggerConfig config{.additive = false};
	try {
		config.sinks.push_back(Logging::createFileAppender("chat.log", "${date} %message%n"));
	} catch (const std::exception& e) {
		// like logback: report the appender that could not be started and continue without it
		std::fprintf(stderr, "Could not open log/chat.log: %s\n", e.what());
	}
	std::string discordPattern = "%replace(%msg){'\\[(.*?) \\((.)\\)\\] (.*?): (.*)', '[$1] $3|" + avatarUrl + "|$4'}";
	if (auto discord = Logging::createDiscordAppender("app_chat_discord_async", webhookUrl, discordPattern))
		config.sinks.push_back(std::move(discord));
	LoggerFactory::configure("CHAT_LOG", std::move(config));
}

void waitForShutdownDone() {
	ShutdownState& s = shutdownState();
	std::unique_lock lock(s.mutex);
	s.changed.wait(lock, [&] { return s.done; });
}

#ifdef _WIN32
BOOL WINAPI consoleCtrlHandler(DWORD ctrlType) {
	switch (ctrlType) {
		case CTRL_C_EVENT:
		case CTRL_BREAK_EVENT:
			requestShutdown();
			return TRUE; // main() shuts the server down and exits
		case CTRL_CLOSE_EVENT:
		case CTRL_LOGOFF_EVENT:
		case CTRL_SHUTDOWN_EVENT:
			requestShutdown();
			waitForShutdownDone(); // Windows terminates the process as soon as this handler returns
			return TRUE;
		default:
			return FALSE;
	}
}
#else
std::atomic<bool> signalReceived = false;

void signalHandler(int) {
	signalReceived.store(true);
}
#endif

/** Java: Runtime.getRuntime().addShutdownHook(new ShutdownHook()) */
void installShutdownHook() {
#ifdef _WIN32
	// a process started with Ctrl+C ignored (inherited from its parent) would never see CTRL_C_EVENT; restore normal processing
	SetConsoleCtrlHandler(nullptr, FALSE);
	if (!SetConsoleCtrlHandler(&consoleCtrlHandler, TRUE))
		log().warn("Could not install the console control handler (error " + std::to_string(GetLastError()) + ")");
#else
	std::signal(SIGINT, &signalHandler);
	std::signal(SIGTERM, &signalHandler);
#endif
}

/** @return true if the stop file exists (it is deleted then) */
bool consumeStopFile(const std::optional<std::filesystem::path>& stopFile) {
	if (!stopFile)
		return false;
	std::error_code error;
	if (!std::filesystem::exists(*stopFile, error))
		return false;
	std::filesystem::remove(*stopFile, error);
	log().info("Stop file " + stopFile->string() + " found, shutting down");
	return true;
}

void waitForShutdownRequest(const std::optional<std::filesystem::path>& stopFile) {
	ShutdownState& s = shutdownState();
	std::unique_lock lock(s.mutex);
	while (!s.requested) {
#ifndef _WIN32
		if (signalReceived.load())
			break;
#endif
		lock.unlock();
		bool stop = consumeStopFile(stopFile);
		lock.lock();
		if (stop)
			break;
		s.changed.wait_for(lock, STOP_FILE_POLL_INTERVAL);
	}
}

} // namespace

Arguments parseArguments(std::span<const std::string_view> args) {
	Arguments arguments;
	constexpr std::string_view stopFileOption = "--stop-file=";
	for (std::string_view arg : args) {
		size_t separator = arg.find('=');
		if (arg.starts_with("-D") && separator != std::string_view::npos && separator > 2)
			arguments.overrides.setProperty(std::string(arg.substr(2, separator - 2)), std::string(arg.substr(separator + 1)));
		else if (arg.starts_with(stopFileOption) && arg.size() > stopFileOption.size())
			arguments.stopFile = std::filesystem::path(std::string(arg.substr(stopFileOption.size())));
		else
			arguments.unknownArguments.emplace_back(arg);
	}
	return arguments;
}

void startup(const Properties& overrides) {
	Logging::init(Config::loadLoggingConfig()); // must run before instantiating any logger (C++: before anything logs to the files)
	configureChatLog();
	commons::utils::concurrent::UncaughtExceptionHandler::install();

	Config::load(overrides);
	commons::database::DatabaseFactory::init(databaseOptions());
	utils::IdFactory::getInstance();
	service::GameServerService::getInstance();
	service::ChatService::getInstance();
	service::BroadcastService::getInstance();

	commons::utils::info::VersionInfo::logAll();
	commons::utils::info::SystemInfo::logAll();

	network::netty::NettyServer::getInstance();
	nettyServerStarted = true;
}

commons::database::DatabaseFactory::Options databaseOptions() {
	return commons::database::DatabaseFactory::Options{
		.socketTimeout = std::nullopt, .defaultSocketTimeout = std::chrono::seconds(60), .requireSocketTimeout = false};
}

void shutdown() {
	if (nettyServerStarted)
		network::netty::NettyServer::getInstance().shutdownAll();
	// shut down logger factory to flush all pending log messages
	Logging::shutdown();
}

void requestShutdown() noexcept {
	ShutdownState& s = shutdownState();
	{
		std::lock_guard lock(s.mutex);
		s.requested = true;
	}
	s.changed.notify_all();
}

int main(std::span<const std::string_view> args) {
	commons::utils::concurrent::setCurrentThreadName("main"); // Java: the main thread's name in log lines
	commons::utils::concurrent::UncaughtExceptionHandler::install();
	Arguments arguments = parseArguments(args);
	try {
		startup(arguments.overrides);
	} catch (...) {
		commons::utils::concurrent::UncaughtExceptionHandler::uncaughtException("main", std::current_exception());
		try {
			shutdown();
		} catch (...) {
			LoggerFactory::flushAll();
		}
		return commons::utils::ExitCode::ERROR_;
	}
	for (const std::string& argument : arguments.unknownArguments)
		log().warn("Unknown command line argument ignored: " + argument);

	installShutdownHook();
	waitForShutdownRequest(arguments.stopFile);

	shutdown();
	{
		std::lock_guard lock(shutdownState().mutex);
		shutdownState().done = true;
	}
	shutdownState().changed.notify_all();
	return commons::utils::ExitCode::NORMAL;
}

} // namespace aion::chatserver::ChatServer
