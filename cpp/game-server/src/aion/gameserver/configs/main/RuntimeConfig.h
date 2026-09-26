#pragma once

#include <atomic>
#include <cstdint>

#include "aion/gameserver/configs/detail/ConfigSupport.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/services/LeakCensus.h"
#include "aion/gameserver/runtime/sync/Watchdog.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"

namespace aion::gameserver::configs::main {

/**
 * Settings of the C++ runtime kernel that the Java server does not have (design runtime-architecture.md §10: gameserver.runtime.*,
 * gameserver.idfactory.*, plus the watchdog, scheduler and debug switches documented in the kernel headers). The keys are optional; a server
 * without them runs with the defaults below, which equal the defaults of the kernel's Config structs.
 * <p>
 * The kernel reads its configuration once (configure() calls in the startup sequence), so a rebinding by Config::load has no effect until the
 * next start, except where a kernel component documents otherwise (ThreadPoolManager coalescing). The fields are std::atomic anyway, because
 * Config::load rebinds every config class while other threads run.
 * <p>
 * The *Config() functions build the kernel Config structs from these fields and the Java config classes they depend on (ThreadConfig).
 * <p>
 * Not in Java.
 */
struct RuntimeConfig {
	/** Reclaimer thread scan period in milliseconds (Reclaimer::Config::period) */
	static inline std::atomic<int32_t> RECLAIM_PERIOD_MILLIS{20};

	/** Retired objects waiting for reclamation above which the watchdog dumps the pinning task (Reclaimer::Config::backlogDumpObjects) */
	static inline std::atomic<int64_t> BACKLOG_DUMP_OBJECTS{1'000'000};

	/** Minutes after removal from the world before the zombie breaker cuts known edges of a still alive object (LeakCensus::Config, D7) */
	static inline std::atomic<int32_t> ZOMBIE_BREAK_MINUTES{30};

	/** Minutes after removal from the world before a still alive object is reported as a leak (LeakCensus::Config::censusAfter) */
	static inline std::atomic<int32_t> LEAK_CENSUS_MINUTES{10};

	/** Object id cursor wrap-around bound, at least 2 (IDFactory::Config::wrapAt, default 2^27) */
	static inline std::atomic<int32_t> IDFACTORY_WRAP_AT{1 << 27};

	/** Seconds a released object id stays quarantined before it can be reused (IDFactory::Config::releaseDelay) */
	static inline std::atomic<int32_t> IDFACTORY_RELEASE_DELAY_SECONDS{300};

	/** Seconds a task may run before the watchdog dumps it as stalled (Watchdog::Config::stall) */
	static inline std::atomic<int32_t> WATCHDOG_STALL_SECONDS{60};

	/** Exit with the RESTART code when the watchdog detects a deadlock (Watchdog::Config::restartOnDeadlock, D5) */
	static inline std::atomic<bool> WATCHDOG_RESTART_ON_DEADLOCK{false};

	/** Periods a fixed-rate task may fall behind before it runs once and realigns; 0 disables (ThreadPoolManager::Config::coalesceAfterPeriods) */
	static inline std::atomic<int32_t> SCHEDULER_COALESCE_AFTER{10};

	/** Run every pool, the packet processor and cron on one thread (ThreadPoolManager::Config::singleExecutor, reproduction aid) */
	static inline std::atomic<bool> DEBUG_SINGLE_EXECUTOR{false};

	/** Run movement ticks sequentially (ThreadPoolManager::Config::serialMovement, reproduction aid) */
	static inline std::atomic<bool> DEBUG_SERIAL_MOVEMENT{false};

	/** Binds the fields above to their property keys. */
	static void bind(commons::configuration::ConfigurableProcessor& processor);

	/** @return Reclaimer::Config{} with period and backlogDumpObjects from this config */
	static runtime::Reclaimer::Config reclaimerConfig();

	/** @return LeakCensus::Config{} with censusAfter and zombieBreakAfter from this config */
	static runtime::LeakCensus::Config leakCensusConfig();

	/** @return IDFactory::Config{} with wrapAt and releaseDelay from this config */
	static utils::idfactory::IDFactory::Config idFactoryConfig();

	/** @return Watchdog::Config{} with slowTaskWarning (ThreadConfig::MAXIMUM_RUNTIME_IN_MILLISEC_WITHOUT_WARNING), stall and restartOnDeadlock */
	static runtime::Watchdog::Config watchdogConfig();

	/**
	 * @return ThreadPoolManager::Config{} with the pool sizes, priorities and slow-task threshold from ThreadConfig and coalescing and the debug
	 *         switches from this config
	 */
	static utils::ThreadPoolManager::Config threadPoolManagerConfig();
};

} // namespace aion::gameserver::configs::main
