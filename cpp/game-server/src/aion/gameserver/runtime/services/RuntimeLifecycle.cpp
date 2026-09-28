#include "aion/gameserver/runtime/services/RuntimeLifecycle.h"

#include <atomic>
#include <mutex>
#include <string>
#include <utility>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sched/ForkJoinPool.h"
#include "aion/gameserver/utils/cron/ThreadPoolManagerRunnableRunner.h"

namespace aion::gameserver::runtime {

namespace {

using services::cron::CronService;
using utils::ThreadPoolManager;
using utils::idfactory::IDFactory;

const commons::logging::Logger& log() {
	static const auto* instance =
		new commons::logging::Logger(commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.runtime.RuntimeLifecycle"));
	return *instance;
}

/** what start() did, so shutdown() and a failed start() undo exactly that */
struct Started {
	bool reclaimerThread = false;
	bool census = false;
	bool cleaner = false;
	bool watchdogThread = false;
	bool pools = false;
	bool cron = false;
};

struct LifecycleState {
	/** serializes start, shutdown and resetForTests (never taken by the kernel services, so they may run while it is held) */
	std::mutex mutex;
	/** written under `mutex`, read lock-free by getState */
	std::atomic<RuntimeLifecycle::State> state{RuntimeLifecycle::State::NEW};
	Started started;
};

LifecycleState& lifecycle() {
	static auto* instance = new LifecycleState(); // leaked: shutdown may run during static destruction of other objects
	return *instance;
}

const char* stateName(RuntimeLifecycle::State state) noexcept {
	switch (state) {
		case RuntimeLifecycle::State::NEW:
			return "new";
		case RuntimeLifecycle::State::STARTING:
			return "starting";
		case RuntimeLifecycle::State::RUNNING:
			return "running";
		case RuntimeLifecycle::State::STOPPING:
			return "stopping";
		case RuntimeLifecycle::State::SHUT_DOWN:
			return "shut down";
	}
	return "unknown";
}

/** runs one shutdown step; a failure is logged and the sequence continues */
template <class F>
void step(const char* name, F&& body) noexcept {
	try {
		body();
	} catch (...) {
		try {
			log().errorCurrentException(std::string("Runtime shutdown step failed: ") + name);
		} catch (...) {
		}
	}
}

/** design §11 step 6: destroys what the cancelled tasks released and hands their auto-release ids to the cleaner action */
size_t finalCleanerDrain() {
	Reclaimer& reclaimer = Reclaimer::getInstance();
	size_t drained = 0;
	for (int round = 0; round < 64; ++round) {
		(void)reclaimer.drain();
		size_t processed = CleanerQueue::drainNow();
		drained += processed;
		if (processed == 0 && CleanerQueue::isEmpty())
			break;
	}
	return drained;
}

/** shutdown steps 1-5 for the services in `started`; fills the report */
void stopServices(Started& started, RuntimeLifecycle::ShutdownReport& report) noexcept {
	if (started.cron) {
		step("CronService", [] { CronService::getInstance().shutdown(); });
		started.cron = false;
	}
	if (started.pools) {
		step("ThreadPoolManager", [&report] {
			ThreadPoolManager::getInstance().shutdown();
			if (ExecutorBackend* backend = ThreadPoolManager::installedBackend())
				report.tasksLeft = backend->pendingTasks().size();
		});
		started.pools = false;
	}
	if (started.cleaner) {
		step("CleanerQueue", [] { CleanerQueue::uninstall(); });
		started.cleaner = false;
	}
	step("final CleanerDrain", [&report] { report.cleanerIdsDrained = finalCleanerDrain(); });
	step("LeakCensus", [&report, &started] {
		if (!started.census)
			return;
		LeakCensus& census = LeakCensus::getInstance();
		report.censusTracked = census.trackedCount();
		if (report.censusTracked != 0) {
			log().warn("Runtime shutdown: {} objects removed from the world are still alive", report.censusTracked);
			for (const LeakCensus::LeakReport& leak : census.getLeaks())
				log().warn("\t... {} (object ID {}, refCount {}, removed {} s ago, {} pinning tasks)", leak.className, leak.objectId, leak.refCount,
					leak.removedFor.count(), leak.pinningTasks.size());
		}
	});
	step("Reclaimer stats", [&report] { report.reclaimerBacklog = Reclaimer::getInstance().stats().backlog; });
	if (started.watchdogThread) {
		step("Watchdog", [] { Watchdog::getInstance().stop(); });
		started.watchdogThread = false;
	}
	if (started.reclaimerThread) {
		step("Reclaimer", [] { Reclaimer::getInstance().stop(); });
		started.reclaimerThread = false;
	}
	if (started.census) {
		step("LeakCensus uninstall", [] {
			LeakCensus::getInstance().uninstall();
			// a Reclaimer thread that start() did not start may still scan: a finished reclaimNow proves no census hook is in flight any more
			if (Reclaimer::getInstance().isRunning())
				Reclaimer::getInstance().reclaimNow();
		});
		started.census = false;
	}
}

} // namespace

void RuntimeLifecycle::start(Options options) {
	LifecycleState& s = lifecycle();
	std::scoped_lock lock(s.mutex);
	State current = s.state.load(std::memory_order_acquire);
	if (current != State::NEW)
		throw IllegalStateException(std::string("RuntimeLifecycle::start: the runtime is ") + stateName(current) +
			" (one start per process; tests call resetForTests first)");
	s.state.store(State::STARTING, std::memory_order_release);
	Started& started = s.started;
	started = Started{};
	try {
		// 1. Reclaimer
		Reclaimer& reclaimer = Reclaimer::getInstance();
		if (options.startReclaimerThread) {
			started.reclaimerThread = !reclaimer.isRunning();
			reclaimer.start(options.reclaimer);
		} else {
			reclaimer.configure(options.reclaimer);
		}

		// 2. LeakCensus and CleanerQueue hooks
		LeakCensus& census = LeakCensus::getInstance();
		census.configure(options.leakCensus);
		census.install();
		started.census = true;
		CleanerQueue::setCleanerAction(options.cleanerAction);
		CleanerQueue::install();
		started.cleaner = true;

		// 3. Watchdog
		Watchdog& watchdog = Watchdog::getInstance();
		if (options.startWatchdog) {
			started.watchdogThread = !watchdog.isRunning();
			watchdog.start(options.watchdog);
		} else {
			watchdog.configure(options.watchdog);
		}

		// 4. ThreadPoolManager
		if (ExecutorBackend* previous = ThreadPoolManager::installedBackend()) {
			if (!previous->isShutdown())
				log().warn("RuntimeLifecycle::start: retiring a thread pool backend created before the runtime start (pending tasks are cancelled)");
			ThreadPoolManager::installBackend(nullptr);
		}
		ThreadPoolManager::configure(options.threadPool);
		const bool explicitBackend = options.backend != nullptr;
		if (explicitBackend)
			ThreadPoolManager::installBackend(std::move(options.backend));
		(void)ThreadPoolManager::getInstance(); // creates the default pools unless a backend was installed
		started.pools = true;
		ForkJoinPool::commonPool().setSerial(
			options.serialForkJoin.value_or(options.threadPool.singleExecutor || options.threadPool.serialMovement || explicitBackend));

		// 5. CronService
		CronService::Driver driver = options.cronDriver;
		if (driver == CronService::Driver::AUTO && explicitBackend)
			driver = CronService::Driver::EXECUTOR;
		CronService::initSingleton(std::make_unique<utils::cron::ThreadPoolManagerRunnableRunner>(), options.cronTimeZone, driver);
		started.cron = true;

		// 6. IDFactory
		IDFactory& idFactory = IDFactory::getInstance();
		idFactory.configure(options.idFactory);
		for (const UsedIdsSource& source : options.usedIds) {
			if (!source.load)
				continue;
			std::vector<int32_t> ids = source.load();
			if (log().isDebugEnabled())
				log().debug("IDFactory: locking {} used IDs of {}", ids.size(), source.name);
			idFactory.lockIds(ids);
		}
		idFactory.logUsedCount();
	} catch (...) {
		ShutdownReport ignored;
		stopServices(started, ignored);
		s.state.store(State::SHUT_DOWN, std::memory_order_release);
		throw;
	}
	s.state.store(State::RUNNING, std::memory_order_release);
}

RuntimeLifecycle::ShutdownReport RuntimeLifecycle::shutdown() {
	if (TaskScope::active())
		throw IllegalStateException("RuntimeLifecycle::shutdown must not be called inside a TaskScope (post it to the ShutdownHook thread)");
	LifecycleState& s = lifecycle();
	std::scoped_lock lock(s.mutex);
	ShutdownReport report;
	if (s.state.load(std::memory_order_acquire) != State::RUNNING)
		return report;
	s.state.store(State::STOPPING, std::memory_order_release);
	report.performed = true;
	stopServices(s.started, report);
	s.state.store(State::SHUT_DOWN, std::memory_order_release);
	return report;
}

RuntimeLifecycle::State RuntimeLifecycle::getState() noexcept {
	return lifecycle().state.load(std::memory_order_acquire);
}

void RuntimeLifecycle::resetForTests() {
	LifecycleState& s = lifecycle();
	std::scoped_lock lock(s.mutex);
	State current = s.state.load(std::memory_order_acquire);
	if (current != State::NEW && current != State::SHUT_DOWN)
		throw IllegalStateException(std::string("RuntimeLifecycle::resetForTests: the runtime is ") + stateName(current));
	CronService::resetForTests();
	ThreadPoolManager::installBackend(nullptr);
	ThreadPoolManager::configure(ThreadPoolManager::Config{});
	ForkJoinPool::commonPool().setSerial(false);
	LeakCensus::getInstance().uninstall();
	LeakCensus::getInstance().configure(LeakCensus::Config{});
	CleanerQueue::uninstall();
	CleanerQueue::setCleanerAction(nullptr);
	IDFactory::getInstance().resetForTests();
	s.started = Started{};
	s.state.store(State::NEW, std::memory_order_release);
}

} // namespace aion::gameserver::runtime
