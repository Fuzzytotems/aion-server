#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/sched/ExecutorBackend.h"
#include "aion/gameserver/runtime/services/CleanerQueue.h"
#include "aion/gameserver/runtime/services/LeakCensus.h"
#include "aion/gameserver/runtime/sync/Watchdog.h"
#include "aion/gameserver/services/cron/CronService.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"

namespace aion::gameserver::runtime {

/**
 * Starts and stops the runtime kernel services in one place, for the S0a main.cpp, GameServer::main and ShutdownHook (P5-14) and test harnesses
 * (design runtime-architecture.md §1.1, §5.4, §6, §7, §11). No Java counterpart: it bundles the kernel part of Java
 * GameServer.initUtilityServicesAndConfig + IDFactory.getInstance() and the tail of ShutdownHook.run, plus the C++-only services (Reclaimer,
 * LeakCensus, CleanerQueue, Watchdog).
 *
 * start(options), called by GameServer::main after Config.load, DatabaseFactory.init and PlayerDAO.setAllPlayersOffline (Java order):
 * 1. Reclaimer: configure, start the thread (Java: the JVM's GC exists before main).
 * 2. LeakCensus configure + install, CleanerQueue cleaner action + install (their Reclaimer hooks must exist before the first game object).
 * 3. Watchdog start (Java: DeadLockDetector.start in the ThreadPoolManager constructor, before the pools).
 * 4. ThreadPoolManager: configure, then install `options.backend` or create the default pools (Java: ThreadPoolManager.getInstance(), which
 *    logs "ThreadPoolManager: Initialized with ..."). A backend installed before start() (or pools created lazily by earlier code) is retired
 *    first, with a warning unless it was already shut down. ForkJoinPool::commonPool() serial mode is set (see Options::serialForkJoin).
 * 5. CronService.initSingleton(ThreadPoolManagerRunnableRunner, time zone) (Java: last step of initUtilityServicesAndConfig).
 * 6. IDFactory: configure, lockIds(0) (getInstance), lockIds of every used-ids source in order (Java IDFactory.initializeUsedIds: PlayerDAO,
 *    InventoryDAO, PlayerRegisteredItemsDAO, LegionDAO, MailDAO, GuideDAO, HousesDAO, PlayerPetsDAO), then "IDFactory: N IDs used.".
 * If a step throws, the steps already done are undone like shutdown() does (cron and pools shut down, hooks uninstalled, threads started by
 * start() stopped), the state becomes SHUT_DOWN and the exception propagates (Java: the Error ends the process).
 *
 * shutdown(), called by ShutdownHook after its game steps (network, PeriodicSaveService.onShutdown, GameTimeService.saveGameTime):
 * 1. CronService.shutdown() (pending jobs dropped, their pins released).
 * 2. ThreadPoolManager.shutdown() (Java log lines; delayed tasks cancelled, running and queued tasks get 5 s). The shut-down backend stays
 *    installed, so later submissions are cancelled and dropped instead of creating new pools.
 * 3. Final CleanerDrain (design §11 step 6): the CleanerQueue hook is removed, then Reclaimer::drain() and CleanerQueue::drainNow() alternate
 *    until no id is left (captures released by the cancelled tasks are destroyed and their auto-release ids reach IDFactory).
 * 4. LeakCensus check: objects removed from the world that are still alive are logged (one warning with the count, then the leak reports).
 * 5. Watchdog and Reclaimer threads are stopped if start() started them; LeakCensus is uninstalled.
 * Each step logs and swallows its own exception, so the later steps still run. Logging::shutdown and quick_exit stay with the caller (§11).
 *
 * Rules
 * - One run per process: start() is allowed only in state NEW (IllegalStateException otherwise, also after shutdown); shutdown() performs the
 *   sequence only in state RUNNING and otherwise returns a report with performed == false (a second call, a call before start, a call racing
 *   with another shutdown). Calls are serialized: a shutdown() during start() waits for start() to finish. resetForTests() returns to NEW.
 * - shutdown() must be called outside any TaskScope (the ShutdownHook thread, main after its phases), because the final drain cannot free
 *   objects the calling task may have borrowed; inside a scope it throws IllegalStateException. Pool tasks and cron jobs run inside scopes, so
 *   they post the shutdown to the ShutdownHook thread instead (design §1.4).
 * - start() may run inside a STARTUP/MAIN TaskScope; the used-ids sources run on the calling thread (DAO calls block inline, §1.5).
 * Thread-safety: all members are thread-safe (serialized by an internal mutex; no lock is held by the kernel services while it is taken).
 */
class RuntimeLifecycle {
public:
	/** One DAO's used object ids (Java DAO getUsedIDs). */
	struct UsedIdsSource {
		/** for the debug log (static storage, e.g. "PlayerDAO") */
		const char* name = "";
		std::function<std::vector<int32_t>()> load;
	};

	/**
	 * Configuration of the kernel services. The runtime does not depend on aion_gs_configs, so GameServer::main builds the options from the
	 * loaded configs:
	 * <pre>
	 * options.reclaimer = RuntimeConfig::reclaimerConfig();          // gameserver.runtime.reclaim_period_ms, backlog_dump_objects
	 * options.leakCensus = RuntimeConfig::leakCensusConfig();        // gameserver.debug.leak_census_minutes, gameserver.runtime.zombie_break_minutes
	 * options.watchdog = RuntimeConfig::watchdogConfig();            // ThreadConfig.MAXIMUM_RUNTIME_IN_MILLISEC_WITHOUT_WARNING,
	 *                                                                // gameserver.watchdog.stall_seconds, restart_on_deadlock
	 * options.threadPool = RuntimeConfig::threadPoolManagerConfig(); // ThreadConfig.*, gameserver.scheduler.coalesce_after,
	 *                                                                // gameserver.debug.single_executor, serial_movement
	 * options.idFactory = RuntimeConfig::idFactoryConfig();          // gameserver.idfactory.wrap_at, release_delay
	 * options.cronTimeZone = GSConfig::TIME_ZONE_ID.load();          // gameserver.timezone
	 * options.usedIds = {{"PlayerDAO", [] { return PlayerDAO::getUsedIDs(); }}, ...};   // Java IDFactory.initializeUsedIds order
	 * options.cleanerAction = ...;                                   // the AionObject Cleaner body, once RespawnService is ported
	 * </pre>
	 */
	struct Options {
		Reclaimer::Config reclaimer;
		/** false: configure only, no Reclaimer thread (DeterministicExecutor tests reclaim with reclaimNow after each task) */
		bool startReclaimerThread = true;

		LeakCensus::Config leakCensus;
		/** CleanerQueue action; nullptr = the default (IDFactory::releaseId) */
		CleanerQueue::CleanerAction cleanerAction = nullptr;

		Watchdog::Config watchdog;
		/** false: configure only (deterministic tests; checkNow still works) */
		bool startWatchdog = true;

		utils::ThreadPoolManager::Config threadPool;
		/** installed instead of the default pools (tests: DeterministicExecutor); nullptr = the pools of `threadPool` */
		std::unique_ptr<ExecutorBackend> backend;
		/**
		 * ForkJoinPool::commonPool() serial mode. Default: serial if threadPool.singleExecutor, threadPool.serialMovement (the only parallel
		 * steady-state user is movement; startup parallel phases then run serially too) or an explicit backend is installed (a deterministic
		 * harness runs ForkJoin work on the executor thread, handlers-and-porting-plan.md amendment §13).
		 */
		std::optional<bool> serialForkJoin;

		/** Java TimeZone.getTimeZone(GSConfig.TIME_ZONE_ID); nullptr = the system time zone */
		const std::chrono::time_zone* cronTimeZone = nullptr;
		/** AUTO: EXECUTOR if an explicit backend is installed (ManualClock fires jobs) or threadPool.singleExecutor, else THREAD */
		services::cron::CronService::Driver cronDriver = services::cron::CronService::Driver::AUTO;

		utils::idfactory::IDFactory::Config idFactory;
		/** locked in this order before the first nextId() */
		std::vector<UsedIdsSource> usedIds;
	};

	enum class State : uint8_t { NEW, STARTING, RUNNING, STOPPING, SHUT_DOWN };

	/** What shutdown() did and found. */
	struct ShutdownReport {
		/** false if shutdown() did nothing (not running) */
		bool performed = false;
		/** tasks the backend still listed as pending after ThreadPoolManager::shutdown (running tasks past the 5 s are not listed) */
		size_t tasksLeft = 0;
		/** ids released by the final CleanerDrain */
		size_t cleanerIdsDrained = 0;
		/** Reclaimer backlog after the final drain (objects still referenced, e.g. by statics or a task that did not finish) */
		uint64_t reclaimerBacklog = 0;
		/** objects removed from the world and still alive after the final drain (LeakCensus::trackedCount) */
		size_t censusTracked = 0;
	};

	/** @throws IllegalStateException if not in state NEW; any exception of a failed step (after the rollback) */
	static void start(Options options);

	/** @throws IllegalStateException if called inside a TaskScope */
	static ShutdownReport shutdown();

	static State getState() noexcept;
	static bool isRunning() noexcept { return getState() == State::RUNNING; }

	/**
	 * Test support: returns to NEW so start() may run again in the same process. Resets CronService, the ThreadPoolManager backend and
	 * configuration (installBackend(nullptr)), LeakCensus and CleanerQueue configuration, IDFactory (resetForTests) and ForkJoin serial mode.
	 * @throws IllegalStateException while starting, running or stopping
	 */
	static void resetForTests();
};

} // namespace aion::gameserver::runtime
