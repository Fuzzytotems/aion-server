#include "aion/loginserver/LoginServer.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <exception>
#include <memory>
#include <mutex>
#include <set>

#ifdef _WIN32
#include <windows.h>
#else
#include <csignal>
#endif

#include "aion/commons/configs/CommonsConfig.h"
#include "aion/commons/configs/DatabaseConfig.h"
#include "aion/commons/configuration/ConfigurableProcessor.h"
#include "aion/commons/configuration/PropertiesUtils.h"
#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/logging/Logging.h"
#include "aion/commons/utils/ExitCode.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/commons/utils/concurrent/ThreadName.h"
#include "aion/commons/utils/concurrent/UncaughtExceptionHandler.h"
#include "aion/commons/utils/info/SystemInfo.h"
#include "aion/commons/utils/info/VersionInfo.h"
#include "aion/loginserver/GameServerTable.h"
#include "aion/loginserver/configs/Config.h"
#include "aion/loginserver/controller/BannedIpController.h"
#include "aion/loginserver/dao/BannedHddDAO.h"
#include "aion/loginserver/dao/BannedMacDAO.h"
#include "aion/loginserver/network/NetConnector.h"
#include "aion/loginserver/network/ncrypt/KeyGen.h"
#include "aion/loginserver/service/PlayerTransferService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::loginserver::LoginServer {

using commons::configuration::ConfigurableProcessor;
using commons::configuration::Properties;
using configs::Config;
namespace Logging = commons::logging::Logging;
namespace LoggerFactory = commons::logging::LoggerFactory;

namespace {

const commons::logging::Logger& log() {
	static const auto* logger = new commons::logging::Logger(LoggerFactory::getLogger("com.aionemu.loginserver.LoginServer"));
	return *logger;
}

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

/** true once PlayerTransferService was created, so a failed startup does not create it just to shut it down */
std::atomic<bool> playerTransferServiceStarted = false;

/** logback.xml: &lt;logger name="...CM_LOGIN" level="debug" additivity="false"&gt; with the app_login_audit appender */
void configureLoginAuditLog() {
	commons::logging::LoggerConfig config{.level = spdlog::level::debug, .additive = false};
	try {
		config.sinks.push_back(Logging::createFileAppender("cm_login.log", "${date} %message%n"));
	} catch (const std::exception& e) {
		// like logback: report the appender that could not be started and continue without it
		std::fprintf(stderr, "Could not open log/cm_login.log: %s\n", e.what());
	}
	LoggerFactory::configure("com.aionemu.loginserver.network.aion.clientpackets.CM_LOGIN", std::move(config));
}

/** C++ addition: binds the configuration again with the override properties layered over the configuration files (same files as Config::load). */
void applyOverrides(const Properties& overrides) {
	namespace PropertiesUtils = commons::configuration::PropertiesUtils;
	auto defaults = std::make_shared<Properties>();
	PropertiesUtils::loadFromDirectory(*defaults, "./config/main", false);
	PropertiesUtils::loadFromDirectory(*defaults, "./config/network", false);
	auto myls = std::make_shared<Properties>(PropertiesUtils::load("./config/myls.properties", defaults));
	Properties properties(myls);
	properties.putAll(overrides);
	std::set<std::string> unused =
		ConfigurableProcessor::process(properties, {&Config::bind, &commons::configs::CommonsConfig::bind, &commons::configs::DatabaseConfig::bind});
	std::vector<std::string> applied;
	for (const std::string& key : overrides.stringPropertyNames()) {
		if (unused.contains(key))
			log().warn("Config property " + key + " is unknown and therefore ignored.");
		else
			applied.push_back(key);
	}
	if (!applied.empty())
		log().info("Applied override properties from the command line: " + commons::utils::StringUtils::join(applied, ", "));
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

void waitForShutdownRequest() {
	ShutdownState& s = shutdownState();
	std::unique_lock lock(s.mutex);
	while (!s.requested) {
#ifndef _WIN32
		if (signalReceived.load())
			break;
#endif
		s.changed.wait_for(lock, std::chrono::milliseconds(200));
	}
}

} // namespace

Properties parseOverrides(std::span<const std::string_view> args, std::vector<std::string>& unknownArguments) {
	Properties overrides;
	for (std::string_view arg : args) {
		size_t separator = arg.find('=');
		if (arg.starts_with("-D") && separator != std::string_view::npos && separator > 2)
			overrides.setProperty(std::string(arg.substr(2, separator - 2)), std::string(arg.substr(separator + 1)));
		else
			unknownArguments.emplace_back(arg);
	}
	return overrides;
}

void startup(const Properties& overrides) {
	Logging::init(Config::loadLoggingConfig()); // must run before anything logs to the files
	configureLoginAuditLog();
	commons::utils::concurrent::UncaughtExceptionHandler::install();

	Config::load();
	if (!overrides.isEmpty())
		applyOverrides(overrides);
	commons::database::DatabaseFactory::init();
	network::ncrypt::KeyGen::init();

	GameServerTable::load();
	controller::BannedIpController::start();
	dao::BannedMacDAO::cleanExpiredBans();
	dao::BannedHddDAO::cleanExpiredBans();

	service::PlayerTransferService::getInstance();
	playerTransferServiceStarted = true;

	commons::utils::info::VersionInfo::logAll();
	commons::utils::info::SystemInfo::logAll();

	network::NetConnector::connect();
}

void shutdown() {
	if (playerTransferServiceStarted)
		service::PlayerTransferService::getInstance().shutdown();
	network::NetConnector::shutdown();
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
	std::vector<std::string> unknownArguments;
	Properties overrides = parseOverrides(args, unknownArguments);
	try {
		startup(overrides);
	} catch (...) {
		commons::utils::concurrent::UncaughtExceptionHandler::uncaughtException("main", std::current_exception());
		try {
			shutdown();
		} catch (...) {
			LoggerFactory::flushAll();
		}
		return commons::utils::ExitCode::ERROR_;
	}
	for (const std::string& argument : unknownArguments)
		log().warn("Unknown command line argument ignored: " + argument);

	installShutdownHook();
	waitForShutdownRequest();

	shutdown();
	{
		std::lock_guard lock(shutdownState().mutex);
		shutdownState().done = true;
	}
	shutdownState().changed.notify_all();
	return commons::utils::ExitCode::NORMAL;
}

} // namespace aion::loginserver::LoginServer
