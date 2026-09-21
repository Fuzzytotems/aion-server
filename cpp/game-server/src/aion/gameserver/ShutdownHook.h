#pragma once

#include <chrono>
#include <climits>
#include <cstdint>
#include <functional>

#include "aion/gameserver/fwd.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"

namespace aion::gameserver {

/**
 * The game server's shutdown: an announced countdown while players are online, then the network, the data saves and the runtime shut down.
 * <p>
 * C++ notes (runtime-architecture.md §11, docs/deviations/P5-14.md):
 * - Java's JVM shutdown hook mechanics are C++-only members: install() (Runtime.addShutdownHook) registers the console control handler
 *   (Ctrl+C and Ctrl+Break request the shutdown, closing the console requests it and waits for it as long as Windows allows), and exit(code)
 *   (System.exit) starts the one "ShutdownHook" thread, which runs run(), the steps main.cpp added (check-output reports) and then the exit
 *   function (Logging::shutdown and std::quick_exit by default). A second exit() while the hook runs does nothing (Java blocks the caller).
 * - run() opens a SHUTDOWN TaskScope around each game step (the player check and announcement of every countdown second, the network shutdown,
 *   PeriodicSaveService.onShutdown, GameTimeService.saveGameTime); CronService and ThreadPoolManager shut down through
 *   RuntimeLifecycle::shutdown outside any scope, followed by the database pool (Java keeps it open until the JVM ends) and "Runtime shut down"
 *   with the report counts. Java's LoggerContext.stop is the exit function's Logging::shutdown.
 * - The operations run() calls on the game (online players, announcement, network, saves, runtime) can be replaced by tests.
 *
 * @author lord_rex, Neon
 */
class ShutdownHook : public runtime::Immortal {
private:
	static constexpr int32_t UNSET_DELAY = INT_MIN;
	runtime::AtomicInteger remainingSeconds{AION_LOCK_CLASS(ShutdownHook::remainingSeconds), UNSET_DELAY};

public:
	static ShutdownHook& getInstance();

private:
	ShutdownHook();
	~ShutdownHook();

public:
	/** Java Thread.run: this method is run when System.exit is triggered, or via other external events like console CTRL+C */
	void run();

	/** Java protected (GameServer.initShutdown) */
	void initShutdown(int32_t exitCode, int32_t delaySeconds);

	/**
	 * @param remainingSeconds
	 *          - remaining time in seconds, until the shutdown will be performed
	 * @param minInterval
	 *          - minimum interval to be returned (minInterval will equal remainingSeconds if remainingSeconds is shorter)
	 * @param maxInterval
	 *          - maximum interval to be returned
	 * @return The interval (in seconds) to wait until the next announce should be sent to all players.
	 */
	static int32_t nextInterval(int32_t remainingSeconds, int32_t minInterval, int32_t maxInterval);

	/** Java protected (GameServer.isShutdownScheduled) */
	bool isRunning();

	/** Java protected (GameServer.isShuttingDownSoon) */
	int32_t getRemainingSeconds();

	// ---- C++ only: the JVM shutdown hook mechanics (class comment) ----

	/** Java Runtime.getRuntime().addShutdownHook(this): installs the console control handler. Idempotent. */
	void install();

	/** Java System.exit(exitCode): starts the ShutdownHook thread (run, the added steps, the exit function) unless it was started already */
	void exit(int32_t exitCode);

	/** true once exit() started the hook thread */
	bool isExitRequested() const noexcept;

	/** Waits until the hook thread finished run() and the added steps (before its exit function). @return false on timeout */
	bool awaitCompletion(std::chrono::milliseconds timeout);

	/** A step main.cpp adds: before RuntimeLifecycle::shutdown (the players are saved, the pools still run) or after it (reports) */
	using Step = std::function<void()>;
	void setBeforeRuntimeShutdown(Step step);
	void setAfterRuntimeShutdown(Step step);

	/** What run() does to the game; the defaults call the Java services (tests replace them) */
	struct Operations {
		std::function<bool()> worldHasPlayers;
		std::function<void(int32_t remainingSeconds)> announceShutdown;
		std::function<void()> shutdownNetwork;
		std::function<void()> dumpStats;
		std::function<void()> saveData;
		std::function<void()> shutdownRuntime;
		std::function<void(std::chrono::milliseconds)> sleep;
	};
	static Operations defaultOperations();
	static void setOperationsForTests(Operations operations);

	/** The last function of the hook thread (default: Logging::shutdown, then std::quick_exit(exitCode)) */
	using ExitFunction = void (*)(int32_t exitCode);
	static void setExitFunctionForTests(ExitFunction exitFunction);

	/** Test support: joins a finished hook thread, forgets steps, operations and the exit request and resets the countdown */
	static void resetForTests();
};

} // namespace aion::gameserver
