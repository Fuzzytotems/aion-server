// Entry point of the game server (Java: com.aionemu.gameserver.GameServer.main).
//
// Spine step S0a link proof (docs/design/handlers-and-porting-plan.md §2.5): the executable links every chunk library and the handler
// registries, and runs the ported part of Java's startup in Java order: Logging, Config.load, DatabaseFactory.init, the runtime kernel
// (ThreadPoolManager, CronService, IDFactory and the C++-only Reclaimer, LeakCensus, CleanerQueue and Watchdog; RuntimeLifecycle), then
// DataManager. DataManager is not ported yet, so its AION_UNPORTED ends the startup: the unported site is logged, the kernel shuts down in order
// and the process exits with ExitCode::ERROR_. P5-14 (aion_gs_app) replaces this file's body with GameServer::main and the ShutdownHook.
//
// Run it with ../game-server (the Java module directory) as working directory, so ./config and ./log resolve like for the Java server.
// Configuration properties can be overridden with -D<key>=<value> arguments; they are applied over config/mygs.properties.

#include <exception>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "aion/commons/configuration/Properties.h"
#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/logging/Logger.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/logging/Logging.h"
#include "aion/commons/utils/ExitCode.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/commons/utils/concurrent/ThreadName.h"
#include "aion/commons/utils/concurrent/UncaughtExceptionHandler.h"
#include "aion/gameserver/configs/Config.h"
#include "aion/gameserver/configs/main/GSConfig.h"
#include "aion/gameserver/configs/main/RuntimeConfig.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/services/RuntimeLifecycle.h"

namespace {

using aion::commons::configuration::Properties;
using aion::gameserver::runtime::RuntimeLifecycle;
namespace Logging = aion::commons::logging::Logging;
namespace LoggerFactory = aion::commons::logging::LoggerFactory;
namespace UncaughtExceptionHandler = aion::commons::utils::concurrent::UncaughtExceptionHandler;

const aion::commons::logging::Logger& log() {
	static const auto* logger = new aion::commons::logging::Logger(LoggerFactory::getLogger("com.aionemu.gameserver.GameServer"));
	return *logger;
}

/** -Dkey=value arguments (like the login server); everything else is returned in unknownArguments. */
Properties parseOverrides(int argc, char* argv[], std::vector<std::string>& unknownArguments) {
	Properties overrides;
	for (int i = 1; i < argc; i++) {
		std::string_view arg = argv[i];
		size_t separator = arg.find('=');
		if (arg.starts_with("-D") && separator != std::string_view::npos && separator > 2)
			overrides.setProperty(std::string(arg.substr(2, separator - 2)), std::string(arg.substr(separator + 1)));
		else
			unknownArguments.emplace_back(arg);
	}
	return overrides;
}

/** The kernel configuration from the loaded configs (RuntimeLifecycle::Options documents the keys). */
RuntimeLifecycle::Options runtimeOptions() {
	using aion::gameserver::configs::main::GSConfig;
	using aion::gameserver::configs::main::RuntimeConfig;
	RuntimeLifecycle::Options options;
	options.reclaimer = RuntimeConfig::reclaimerConfig();
	options.leakCensus = RuntimeConfig::leakCensusConfig();
	options.watchdog = RuntimeConfig::watchdogConfig();
	options.threadPool = RuntimeConfig::threadPoolManagerConfig();
	options.idFactory = RuntimeConfig::idFactoryConfig();
	options.cronTimeZone = GSConfig::TIME_ZONE_ID.load();
	// options.usedIds: the DAOs' getUsedIDs (Java IDFactory.initializeUsedIds order) arrive with P4-14
	return options;
}

void startup(const Properties& overrides, const std::vector<std::string>& unknownArguments) {
	using aion::gameserver::configs::Config;
	Logging::init(Config::loadLoggingConfig()); // must run before anything logs to the files

	// C++ addition: command line overrides are layered where Java layers the active events' properties (over mygs.properties), so they also
	// survive later Config::load calls. EventService (P5-12b) must keep them when it registers its provider.
	if (!overrides.isEmpty())
		Config::setEventConfigPropertiesProvider([overrides] { return overrides; });
	Config::load();
	if (!overrides.isEmpty()) {
		std::set<std::string> keys = overrides.stringPropertyNames();
		log().info("Override properties from the command line (unknown keys are warned above): " + aion::commons::utils::StringUtils::join({keys.begin(), keys.end()}, ", "));
	}
	for (const std::string& argument : unknownArguments)
		log().warn("Unknown command line argument ignored: " + argument);

	aion::commons::database::DatabaseFactory::init(aion::commons::database::DatabaseFactory::gameServerOptions());
	// Java: PlayerDAO.setAllPlayersOffline() and DatabaseCleaningService (P4-14, P5-14) come here

	RuntimeLifecycle::start(runtimeOptions());

	{
		using aion::gameserver::runtime::TaskKind;
		aion::gameserver::runtime::TaskScope scope(AION_TASK_INFO(TaskKind::STARTUP));
		aion::gameserver::dataholders::DataManager::getInstance();
	}
	log().info("S0a startup sequence complete (the rest of GameServer.main is not ported yet)");
}

void shutdown() noexcept {
	try {
		RuntimeLifecycle::ShutdownReport report = RuntimeLifecycle::shutdown(); // outside any TaskScope
		if (report.performed)
			log().info("Runtime shut down: {} tasks left, {} cleaner ids drained, reclaimer backlog {}, {} objects still tracked", report.tasksLeft,
				report.cleanerIdsDrained, report.reclaimerBacklog, report.censusTracked);
		if (aion::commons::database::DatabaseFactory::isInitialized())
			aion::commons::database::DatabaseFactory::shutdown();
	} catch (...) {
		UncaughtExceptionHandler::uncaughtException("main", std::current_exception());
	}
	LoggerFactory::flushAll();
	Logging::shutdown();
}

} // namespace

int main(int argc, char* argv[]) {
	aion::commons::utils::concurrent::setCurrentThreadName("main"); // Java: the main thread's name in log lines
	UncaughtExceptionHandler::install();
	std::vector<std::string> unknownArguments;
	Properties overrides = parseOverrides(argc, argv, unknownArguments);
	int exitCode = aion::commons::utils::ExitCode::ERROR_;
	try {
		startup(overrides, unknownArguments);
		exitCode = aion::commons::utils::ExitCode::NORMAL;
	} catch (const aion::gameserver::runtime::UnportedException& e) {
		// the site was already logged with its stack trace by AION_UNPORTED
		log().error("Game server startup stopped at an unported function: " + std::string(e.what()));
	} catch (...) {
		UncaughtExceptionHandler::uncaughtException("main", std::current_exception());
	}
	shutdown();
	return exitCode;
}
