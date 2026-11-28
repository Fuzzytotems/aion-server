#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "aion/gameserver/fwd.h"
#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sync/Monitor.h"

namespace aion::commons::network {
class NioServer;
} // namespace aion::commons::network

namespace aion::commons::utils::info {
class VersionInfo;
} // namespace aion::commons::utils::info

namespace aion::gameserver {

/**
 * <tt>GameServer</tt> is the main class of the application and represents the whole game server.<br>
 * This class is also an entry point with main() method.
 * <p>
 * C++ notes:
 * - main() runs Java's startup in Java order as numbered steps and logs "startup step N: name" before each (m5a-plan.md F-01a); each step
 *   runs in its own STARTUP TaskScope, so the Reclaimer frees what one step unlinked before the next starts. Logging::init (Java's static
 *   initializer), the command line and the check modes stay in main.cpp, which passes a StartupObserver.
 * - Java's parallel stream over the six engines runs them one after another in the stream's source order (like the M4 startup did for
 *   ZoneService and GeoService; docs/deviations/P5-14.md). A StartupObserver may skip the four handler engines (the --check-* modes of M4).
 * - initUtilityServicesAndConfig: PropertyTransformers.register(CronExpressionTransformer) is a compile-time transformer specialization; the
 *   thread pools and CronService start through runtime::RuntimeLifecycle (runtime-architecture.md §23), which configures IDFactory; initDatabase
 *   locks its used ids. Java runs initDatabase concurrently with DataManager.getInstance(); C++ runs it as the step before (P5-14.md).
 * - Every Java statement of main() is wired; F-01b (stage 2) makes the whole startup pass with 0 AION_UNPORTED hits.
 * - The NioServer of the game clients is created once and never freed (LoginServer and ChatServer keep a plain pointer to it for the whole
 *   run); shutdownNioServer() shuts it down and keeps the pointer like Java (isStarting() is "no NioServer yet"). The staff clients it sends to the server selection screen
 *   first get 200 ms (Java: LockSupport.parkNanos) for the login server's reconnection keys.
 * - `System.gc()` has no counterpart.
 *
 * @author -Nemesiss-, SoulKeeper, cura, Neon
 */
class GameServer {
public:
	/** Java: private static final long START_TIME_NANOS = System.nanoTime() (the start of the startup message's time) */
	static const std::chrono::steady_clock::time_point START_TIME_NANOS;

	/** Java: (int) (ManagementFactory.getRuntimeMXBean().getStartTime() / 1000): the process start time in seconds since the epoch */
	static const int32_t START_TIME_SECONDS;

	/** Java: public static final VersionInfo versionInfo = new VersionInfo(GameServer.class) (created on first use, hub-headers.md §11.1) */
	static const commons::utils::info::VersionInfo& versionInfo();

	/** C++ only: the hooks of main.cpp into the startup (check modes, measurements). The defaults run the Java startup unchanged. */
	class StartupObserver {
	public:
		virtual ~StartupObserver() = default;

		/**
		 * Runs one startup step; GameServer has logged "startup step N: name" and opened the step's STARTUP TaskScope. The default runs body().
		 */
		virtual void runStep(int32_t number, std::string_view name, const std::function<void()>& body);

		/** after Config.load (e.g. to log the command line overrides) */
		virtual void afterConfigLoad() {}

		/** false skips the handler engines QuestEngine, AIEngine, InstanceEngine and ChatProcessor (the M4 check modes) */
		virtual bool initHandlerEngines() { return true; }

		/** after the runtime (thread pools, CronService) started: false ends the startup here */
		virtual bool continueAfterRuntime() { return true; }

		/** after initDatabase (DatabaseFactory, PlayerDAO.setAllPlayersOffline, the cleaning, IDFactory's used ids): false ends the startup here */
		virtual bool continueAfterDatabase() { return true; }

		/** after World was created: false ends the startup here */
		virtual bool continueAfterWorld() { return true; }
	};

private:
	// fieldmap: Java's NioServer reference; the C++ NioServer is no RefCounted object and lives for the whole run (class comment)
	static inline std::atomic<commons::network::NioServer*> nioServer{nullptr};

	// TODO remove all this shit
	static inline runtime::Field<int32_t> ELYOS_COUNT{0};
	static inline runtime::Field<int32_t> ASMOS_COUNT{0};
	static inline runtime::Field<float> ELYOS_RATIO{0.0f};
	static inline runtime::Field<float> ASMOS_RATIO{0.0f};
	static inline runtime::Monitor lock{AION_LOCK_CLASS(GameServer::lock)};

	GameServer() = delete;

public:
	/** Java: static void main() */
	static void main();

	/**
	 * C++ only: main() with the hooks of main.cpp.
	 *
	 * @return true if the whole startup ran ("Game server started"), false if the observer ended it early
	 */
	static bool main(StartupObserver& observer);

private:
	/** Starts servers for connection with aion client and login\chat server. */
	static commons::network::NioServer* initNioServer();

	/**
	 * Initialize all helper services, that are not directly related to aion gs, which includes: Thread pool, Cron service.
	 * This method also initializes Config. The database is initialized afterwards, see initDatabase().
	 *
	 * @return false if the observer ended the startup after the runtime start
	 */
	static bool initUtilityServicesAndConfig(StartupObserver& observer, int32_t& step);

	/** DatabaseFactory.init, PlayerDAO.setAllPlayersOffline, the cleaning service and IDFactory's used ids (upstream 546f96bd2) */
	static void initDatabase();

	/** C++ only: logs "startup step N: name" and runs the body through the observer inside a STARTUP TaskScope */
	static void runStep(StartupObserver& observer, int32_t& step, std::string_view name, const std::function<void()>& body);

public:
	static void shutdownNioServer();

	/** Java: Stream<AionConnection> (the game client connections of NioServer.findAllConnections) */
	static std::vector<std::shared_ptr<network::aion::AionConnection>> findClientConnections();

private:
	static bool forceStaffClientsToServerSelectionScreen();

public:

	static bool isShutdownScheduled();

	/** @return true while the server is starting up and has not opened its port yet */
	static bool isStarting();

	static bool isShuttingDownSoon();

	static void initShutdown(int32_t exitCode, int32_t delaySeconds);

	/** @param race Java null returns at once */
	static void updateRatio(std::optional<model::Race> race, int32_t i);

	static float getRatiosFor(model::Race race);

	static int32_t getCountFor(model::Race race);

	/** C++ only: the client NioServer (null before initNioServer) */
	static commons::network::NioServer* getNioServer() noexcept;
};

} // namespace aion::gameserver
