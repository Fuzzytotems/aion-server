#include "aion/gameserver/ShutdownHook.h"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdlib>
#include <exception>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/logging/Logging.h"
#include "aion/commons/utils/ExitCode.h"
#include "aion/commons/utils/concurrent/RunnableStatsManager.h"
#include "aion/commons/utils/concurrent/ThreadName.h"
#include "aion/commons/utils/concurrent/UncaughtExceptionHandler.h"
#include "aion/gameserver/GameServer.h"
#include "aion/gameserver/configs/main/ShutdownConfig.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/services/RuntimeLifecycle.h"
#include "aion/gameserver/services/GameTimeService.h"
#include "aion/gameserver/services/PeriodicSaveService.h"
#include "aion/gameserver/services/cron/CronExpression.h"
#include "aion/gameserver/services/cron/CronService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/world/World.h"

#ifdef _WIN32
#include <Windows.h>
#endif

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver {

namespace {

const commons::logging::Logger& log() {
	static const auto* logger = new commons::logging::Logger(commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.ShutdownHook"));
	return *logger;
}

void quickExit(int32_t exitCode) {
	commons::logging::LoggerFactory::flushAll();
	commons::logging::Logging::shutdown();
	std::quick_exit(exitCode);
}

/** The C++-only hook state (class comment): the hook thread, the steps main.cpp added, the replaceable operations and the exit function */
struct HookState {
	std::mutex mutex; // confined: ShutdownHook's own state, never held while game code runs
	std::condition_variable completed;
	bool exitRequested = false;
	bool runCompleted = false;
	bool consoleHandlerInstalled = false;
	int32_t exitCode = commons::utils::ExitCode::NORMAL;
	std::thread thread; // lint: L6 the on-demand ShutdownHook thread of runtime-architecture.md §1.1 (Java: the JVM shutdown hook thread)
	ShutdownHook::Step beforeRuntimeShutdown;
	ShutdownHook::Step afterRuntimeShutdown;
	std::unique_ptr<ShutdownHook::Operations> operations;
	ShutdownHook::ExitFunction exitFunction = &quickExit;
	std::atomic<bool> hasExitRequest{false};
};

HookState& state() {
	static auto* hookState = new HookState(); // lint: L5 process-lifetime hook state, never destroyed (the hook thread may outlive main)
	return *hookState;
}

ShutdownHook::Operations operations() {
	HookState& s = state();
	std::scoped_lock lock(s.mutex);
	return s.operations ? *s.operations : ShutdownHook::defaultOperations();
}

#ifdef _WIN32
BOOL WINAPI consoleCtrlHandler(DWORD ctrlType) {
	switch (ctrlType) {
		case CTRL_C_EVENT:
		case CTRL_BREAK_EVENT:
			ShutdownHook::getInstance().exit(commons::utils::ExitCode::NORMAL); // the hook thread runs the countdown; this handler returns
			return TRUE;
		case CTRL_CLOSE_EVENT:
		case CTRL_LOGOFF_EVENT:
		case CTRL_SHUTDOWN_EVENT:
			// Windows terminates the process shortly after this handler returns (about 5 seconds for a closed console): wait as long as it allows
			ShutdownHook::getInstance().exit(commons::utils::ExitCode::NORMAL);
			ShutdownHook::getInstance().awaitCompletion(std::chrono::seconds(30));
			return TRUE;
		default:
			return FALSE;
	}
}
#endif

} // namespace

ShutdownHook& ShutdownHook::getInstance() {
	static ShutdownHook instance; // Java SingletonHolder
	return instance;
}

// lambda at ShutdownHook.java:38 (fieldmap: stored in CronService, pin {}, captures none)
ShutdownHook::ShutdownHook() {
	if (const services::cron::CronExpression* restartSchedule = configs::main::ShutdownConfig::RESTART_SCHEDULE.load()) {
		// CurrentThreadRunnableRunner, otherwise ThreadPoolManager.getInstance().shutdown() will try to wait for this cron task
		services::cron::CronService::getInstance().schedule(services::cron::CronJob([] { ShutdownHook::getInstance().exit(commons::utils::ExitCode::RESTART); }),
			std::make_shared<services::cron::CurrentThreadRunnableRunner>(), *restartSchedule, true);
		log().info("Scheduled automatic server restart based on cron expression: " + restartSchedule->toString());
	}
}

ShutdownHook::~ShutdownHook() = default;

void ShutdownHook::run() {
	Operations ops = operations();
	// this method is run when System.exit is triggered, or via other external events like console CTRL+C
	remainingSeconds.compareAndSet(UNSET_DELAY, configs::main::ShutdownConfig::DELAY.load());
	for (int32_t announceInterval = 1, expectedSeconds = remainingSeconds.get(); remainingSeconds.get() > 0;) {
		try {
			if (!ops.worldHasPlayers())
				break; // fast exit

			if (remainingSeconds.get() % announceInterval == 0) {
				log().info("Runtime is shutting down in " + std::to_string(remainingSeconds.get()) + " seconds.");
				ops.announceShutdown(remainingSeconds.get());
				announceInterval = nextInterval(remainingSeconds.get(), 5, 60);
			}

			ops.sleep(std::chrono::milliseconds(1000));

			// if remainingSeconds got updated from another thread
			int32_t previousSeconds = expectedSeconds--;
			if (!remainingSeconds.compareAndSet(previousSeconds, expectedSeconds)) {
				expectedSeconds = remainingSeconds.get();
				announceInterval = 1;
			}
		} catch (const std::exception&) {
			log().errorCurrentException("");
		}
	}

	ops.shutdownNetwork(); // shuts down network, disconnects cs/ls/all players and saves them

	ops.dumpStats(); // RunnableStatsManager.dumpClassStats(SortBy.AVG)
	ops.saveData();

	Step before;
	{
		HookState& s = state();
		std::scoped_lock lock(s.mutex);
		before = s.beforeRuntimeShutdown;
	}
	if (before)
		before();

	ops.shutdownRuntime();
}

void ShutdownHook::initShutdown(int32_t exitCode, int32_t delaySeconds) {
	if (delaySeconds < 0)
		return;
	// update shutdown delay if possible (unset or more than one second left)
	int32_t previousValue = remainingSeconds.getAndUpdate([delaySeconds](int32_t seconds) { return seconds == UNSET_DELAY || seconds > 1 ? delaySeconds : seconds; });
	if (previousValue == UNSET_DELAY)
		exit(exitCode); // Java: Thread.startVirtualThread(() -> System.exit(exitCode)); exit() starts the hook thread and returns
}

int32_t ShutdownHook::nextInterval(int32_t remainingSecondsValue, int32_t minInterval, int32_t maxInterval) {
	if (remainingSecondsValue < minInterval)
		minInterval = std::max(1, remainingSecondsValue);
	int32_t interval = remainingSecondsValue / 2;
	interval = interval / 5 * 5; // ensure a "clean" interval (dividable by 5, like 5, 10, 15s and so on)
	return std::min(maxInterval, std::max(minInterval, interval));
}

bool ShutdownHook::isRunning() {
	return remainingSeconds.get() != UNSET_DELAY;
}

int32_t ShutdownHook::getRemainingSeconds() {
	return remainingSeconds.get();
}

void ShutdownHook::install() {
	HookState& s = state();
	std::scoped_lock lock(s.mutex);
	if (s.consoleHandlerInstalled)
		return;
	s.consoleHandlerInstalled = true;
#ifdef _WIN32
	// a process started with Ctrl+C ignored (inherited from its parent) would never see CTRL_C_EVENT; restore normal processing
	SetConsoleCtrlHandler(nullptr, FALSE);
	if (!SetConsoleCtrlHandler(&consoleCtrlHandler, TRUE))
		log().warn("Could not install the console control handler (error " + std::to_string(GetLastError()) + ")");
#endif
}

void ShutdownHook::exit(int32_t exitCode) {
	HookState& s = state();
	std::scoped_lock lock(s.mutex);
	if (s.exitRequested)
		return; // Java: a second System.exit blocks while the hooks run; the process ends with the first exit code
	s.exitRequested = true;
	s.hasExitRequest.store(true, std::memory_order_release);
	s.exitCode = exitCode;
	// lint: L6 the on-demand ShutdownHook thread (runtime-architecture.md §1.1, §11); joined only by resetForTests, the process ends in it
	s.thread = std::thread([this] {
		commons::utils::concurrent::setCurrentThreadName("ShutdownHook");
		try {
			run();
		} catch (...) {
			commons::utils::concurrent::UncaughtExceptionHandler::uncaughtException("ShutdownHook", std::current_exception());
		}
		HookState& hook = state();
		Step after;
		ExitFunction exitFunction;
		int32_t code;
		{
			std::scoped_lock hookLock(hook.mutex);
			after = hook.afterRuntimeShutdown;
			exitFunction = hook.exitFunction;
			code = hook.exitCode;
		}
		if (after) {
			try {
				after();
			} catch (...) {
				commons::utils::concurrent::UncaughtExceptionHandler::uncaughtException("ShutdownHook", std::current_exception());
			}
		}
		{
			std::scoped_lock hookLock(hook.mutex);
			hook.runCompleted = true;
		}
		hook.completed.notify_all();
		exitFunction(code);
	});
}

bool ShutdownHook::isExitRequested() const noexcept {
	return state().hasExitRequest.load(std::memory_order_acquire);
}

bool ShutdownHook::awaitCompletion(std::chrono::milliseconds timeout) {
	HookState& s = state();
	std::unique_lock lock(s.mutex);
	return s.completed.wait_for(lock, timeout, [&s] { return s.runCompleted; });
}

void ShutdownHook::setBeforeRuntimeShutdown(Step step) {
	HookState& s = state();
	std::scoped_lock lock(s.mutex);
	s.beforeRuntimeShutdown = std::move(step);
}

void ShutdownHook::setAfterRuntimeShutdown(Step step) {
	HookState& s = state();
	std::scoped_lock lock(s.mutex);
	s.afterRuntimeShutdown = std::move(step);
}

ShutdownHook::Operations ShutdownHook::defaultOperations() {
	Operations ops;
	ops.worldHasPlayers = [] {
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::SHUTDOWN));
		return !world::World::getInstance().getAllPlayers().empty();
	};
	ops.announceShutdown = [](int32_t seconds) {
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::SHUTDOWN));
		utils::PacketSendUtility::broadcastToWorld(network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_SERVER_SHUTDOWN(seconds));
	};
	ops.shutdownNetwork = [] {
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::SHUTDOWN));
		GameServer::shutdownNioServer();
	};
	ops.dumpStats = [] { commons::utils::concurrent::RunnableStatsManager::dumpClassStats(commons::utils::concurrent::RunnableStatsManager::SortBy::AVG); };
	ops.saveData = [] {
		{
			runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::SHUTDOWN));
			services::PeriodicSaveService::getInstance().onShutdown();
		}
		{
			runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::SHUTDOWN));
			services::GameTimeService::getInstance().saveGameTime();
		}
	};
	ops.shutdownRuntime = [] {
		// Java: CronService.getInstance().shutdown(); ThreadPoolManager.getInstance().shutdown(); (outside any TaskScope, RuntimeLifecycle.h)
		runtime::RuntimeLifecycle::ShutdownReport report = runtime::RuntimeLifecycle::shutdown();
		if (report.performed)
			log().info("Runtime shut down: {} tasks left, {} cleaner ids drained, reclaimer backlog {}, {} objects still tracked", report.tasksLeft,
				report.cleanerIdsDrained, report.reclaimerBacklog, report.censusTracked);
		if (commons::database::DatabaseFactory::isInitialized())
			commons::database::DatabaseFactory::shutdown();
	};
	ops.sleep = [](std::chrono::milliseconds duration) { std::this_thread::sleep_for(duration); };
	return ops;
}

void ShutdownHook::setOperationsForTests(Operations operationsValue) {
	HookState& s = state();
	std::scoped_lock lock(s.mutex);
	s.operations = std::make_unique<Operations>(std::move(operationsValue));
}

void ShutdownHook::setExitFunctionForTests(ExitFunction exitFunction) {
	HookState& s = state();
	std::scoped_lock lock(s.mutex);
	s.exitFunction = exitFunction ? exitFunction : &quickExit;
}

void ShutdownHook::resetForTests() {
	HookState& s = state();
	std::thread thread; // lint: L6 takes over the finished hook thread to join it (tests only)
	{
		std::scoped_lock lock(s.mutex);
		thread = std::move(s.thread);
	}
	if (thread.joinable())
		thread.join();
	std::scoped_lock lock(s.mutex);
	s.exitRequested = false;
	s.hasExitRequest.store(false, std::memory_order_release);
	s.runCompleted = false;
	s.exitCode = commons::utils::ExitCode::NORMAL;
	s.beforeRuntimeShutdown = nullptr;
	s.afterRuntimeShutdown = nullptr;
	s.operations.reset();
	s.exitFunction = &quickExit;
	getInstance().remainingSeconds.set(UNSET_DELAY);
}

} // namespace aion::gameserver
