#include "aion/gameserver/configs/main/RuntimeConfig.h"

#include <chrono>

#include "aion/gameserver/configs/detail/Bind.h"
#include "aion/gameserver/configs/main/ThreadConfig.h"

namespace aion::gameserver::configs::main {

void RuntimeConfig::bind(commons::configuration::ConfigurableProcessor& p) {
	AION_BIND(p, "gameserver.runtime.reclaim_period_ms", RECLAIM_PERIOD_MILLIS, "20");
	AION_BIND(p, "gameserver.runtime.backlog_dump_objects", BACKLOG_DUMP_OBJECTS, "1000000");
	AION_BIND(p, "gameserver.runtime.zombie_break_minutes", ZOMBIE_BREAK_MINUTES, "30");
	AION_BIND(p, "gameserver.debug.leak_census_minutes", LEAK_CENSUS_MINUTES, "10");
	AION_BIND(p, "gameserver.idfactory.wrap_at", IDFACTORY_WRAP_AT, "134217728");
	AION_BIND(p, "gameserver.idfactory.release_delay", IDFACTORY_RELEASE_DELAY_SECONDS, "300");
	AION_BIND(p, "gameserver.watchdog.stall_seconds", WATCHDOG_STALL_SECONDS, "60");
	AION_BIND(p, "gameserver.watchdog.restart_on_deadlock", WATCHDOG_RESTART_ON_DEADLOCK, "false");
	AION_BIND(p, "gameserver.scheduler.coalesce_after", SCHEDULER_COALESCE_AFTER, "10");
	AION_BIND(p, "gameserver.debug.single_executor", DEBUG_SINGLE_EXECUTOR, "false");
	AION_BIND(p, "gameserver.debug.serial_movement", DEBUG_SERIAL_MOVEMENT, "false");
}

runtime::Reclaimer::Config RuntimeConfig::reclaimerConfig() {
	runtime::Reclaimer::Config config;
	config.period = std::chrono::milliseconds(RECLAIM_PERIOD_MILLIS.load());
	config.backlogDumpObjects = static_cast<size_t>(BACKLOG_DUMP_OBJECTS.load());
	return config;
}

runtime::LeakCensus::Config RuntimeConfig::leakCensusConfig() {
	runtime::LeakCensus::Config config;
	config.censusAfter = std::chrono::minutes(LEAK_CENSUS_MINUTES.load());
	config.zombieBreakAfter = std::chrono::minutes(ZOMBIE_BREAK_MINUTES.load());
	return config;
}

utils::idfactory::IDFactory::Config RuntimeConfig::idFactoryConfig() {
	utils::idfactory::IDFactory::Config config;
	config.wrapAt = IDFACTORY_WRAP_AT.load();
	config.releaseDelay = std::chrono::seconds(IDFACTORY_RELEASE_DELAY_SECONDS.load());
	return config;
}

runtime::Watchdog::Config RuntimeConfig::watchdogConfig() {
	runtime::Watchdog::Config config;
	config.slowTaskWarning = std::chrono::milliseconds(ThreadConfig::MAXIMUM_RUNTIME_IN_MILLISEC_WITHOUT_WARNING.load());
	config.stall = std::chrono::seconds(WATCHDOG_STALL_SECONDS.load());
	config.restartOnDeadlock = WATCHDOG_RESTART_ON_DEADLOCK.load();
	return config;
}

utils::ThreadPoolManager::Config RuntimeConfig::threadPoolManagerConfig() {
	utils::ThreadPoolManager::Config config;
	config.baseThreadPoolSize = ThreadConfig::BASE_THREAD_POOL_SIZE.load();
	config.scheduledThreadPoolSize = ThreadConfig::SCHEDULED_THREAD_POOL_SIZE.load();
	config.usePriorities = ThreadConfig::USE_PRIORITIES.load();
	config.maximumRuntimeInMillisecWithoutWarning = ThreadConfig::MAXIMUM_RUNTIME_IN_MILLISEC_WITHOUT_WARNING.load();
	config.coalesceAfterPeriods = SCHEDULER_COALESCE_AFTER.load();
	config.singleExecutor = DEBUG_SINGLE_EXECUTOR.load();
	config.serialMovement = DEBUG_SERIAL_MOVEMENT.load();
	return config;
}

} // namespace aion::gameserver::configs::main
