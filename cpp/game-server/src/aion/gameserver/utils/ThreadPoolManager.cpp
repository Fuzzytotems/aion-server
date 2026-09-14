#include "aion/gameserver/utils/ThreadPoolManager.h"

#include <algorithm>
#include <format>
#include <map>
#include <mutex>
#include <thread>
#include <tuple>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/commons/utils/concurrent/PriorityThreadFactory.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/sched/Clock.h"
#include "aion/gameserver/runtime/sched/ForkJoinPool.h"
#include "aion/gameserver/runtime/sched/PoolBackends.h"
#include "aion/gameserver/runtime/sched/detail/SchedRuntime.h"

namespace aion::gameserver::utils {

namespace {

using runtime::ExecutorBackend;

const commons::logging::Logger& log() {
	static const auto* instance = new commons::logging::Logger(commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.utils.ThreadPoolManager"));
	return *instance;
}

/**
 * Process-wide scheduler state (leaked: pool threads may outlive static destruction). `backend` is read lock-free by every submission and by
 * kernel hooks; it is written only under `mutex` by installBackend and the lazy creation of the default pools. A backend that was ever
 * published is never destroyed: installBackend retires it (ExecutorBackend::retire: submissions cancelled, threads joined) BEFORE publishing
 * the replacement, and then keeps it in `retired` for the rest of the process, so a pointer or reference loaded lock-free before the swap
 * stays valid (review finding: use-after-free of a destroyed backend). The values read at run time by all backends are atomics.
 */
struct ManagerState {
	std::mutex mutex;
	/** serializes installBackend calls (held while the previous backend is retired, never by submissions) */
	std::mutex installMutex;
	ThreadPoolManager::Config config;
	std::unique_ptr<ExecutorBackend> owned;
	std::atomic<ExecutorBackend*> backend{nullptr};
	/** retired backends (test installs; production retires at most the lazily created pools), kept until process exit */
	std::vector<std::unique_ptr<ExecutorBackend>> retired;
	/** the default pools were created from the configuration */
	bool defaultPools = false;
	std::atomic<bool> shutdown{false};
	std::atomic<int64_t> maximumRuntime{5000};
	std::atomic<int32_t> coalesceAfterPeriods{10};
	std::atomic<int64_t> coalesceMinimumLagMillis{2000};
};

ManagerState& state() {
	static auto* instance = new ManagerState();
	return *instance;
}

/** creates the Java pools (or the single executor) from the configuration; under the state mutex */
std::unique_ptr<ExecutorBackend> createDefaultBackend(const ThreadPoolManager::Config& config) {
	if (config.singleExecutor) {
		auto backend = std::make_unique<runtime::SingleExecutorBackend>(static_cast<size_t>(std::max(1, config.instantQueueCapacity)));
		runtime::ForkJoinPool::commonPool().setSerial(true);
		log().info("ThreadPoolManager: Initialized with 1 thread for all pools (gameserver.debug.single_executor)");
		return backend;
	}
	int32_t availableProcessors = static_cast<int32_t>(std::max(1u, std::thread::hardware_concurrency()));
	runtime::ThreadPoolBackend::Options options;
	options.instantThreads = std::max(4, config.baseThreadPoolSize == 0 ? availableProcessors : config.baseThreadPoolSize);
	options.scheduledThreads = std::max(4, config.scheduledThreadPoolSize == 0 ? availableProcessors : config.scheduledThreadPoolSize);
	options.instantThreadPriority = config.usePriorities ? 7 : commons::utils::concurrent::NORM_PRIORITY;
	options.instantQueueCapacity = static_cast<size_t>(std::max(1, config.instantQueueCapacity));
	auto backend = std::make_unique<runtime::ThreadPoolBackend>(options);
	log().info("ThreadPoolManager: Initialized with {} instant, {} scheduler and {} long running threads", backend->getInstantPoolSize(),
		backend->getScheduledPoolSize(), backend->getLongRunningPoolSize());
	return backend;
}

void applyRuntimeValues(ManagerState& s, const ThreadPoolManager::Config& config) noexcept {
	s.maximumRuntime.store(config.maximumRuntimeInMillisecWithoutWarning, std::memory_order_release);
	s.coalesceAfterPeriods.store(config.coalesceAfterPeriods, std::memory_order_release);
	s.coalesceMinimumLagMillis.store(config.coalesceMinimumLag.count(), std::memory_order_release);
}

/** the installed backend, creating the default pools on first use */
ExecutorBackend& ensureBackend() {
	ManagerState& s = state();
	if (ExecutorBackend* backend = s.backend.load(std::memory_order_acquire))
		return *backend;
	std::scoped_lock lock(s.mutex);
	if (!s.owned) {
		s.owned = createDefaultBackend(s.config);
		s.defaultPools = true;
		s.backend.store(s.owned.get(), std::memory_order_release);
	}
	return *s.owned;
}

size_t countPool(const std::vector<FutureRef>& tasks, runtime::PoolKind pool) {
	return static_cast<size_t>(std::count_if(tasks.begin(), tasks.end(), [pool](const FutureRef& task) { return task->getPool() == pool; }));
}

} // namespace

} // namespace aion::gameserver::utils

// ------------------------------------------------------------------------------------------------------------------------ sched_detail glue

namespace aion::gameserver::runtime::sched_detail {

ExecutorBackend* installedBackend() noexcept {
	return utils::state().backend.load(std::memory_order_acquire);
}

const Clock& schedulerClock() noexcept {
	ExecutorBackend* backend = installedBackend();
	return backend != nullptr ? backend->clock() : SystemClock::getInstance();
}

int64_t maximumRuntimeWithoutWarningMillis() noexcept {
	return utils::state().maximumRuntime.load(std::memory_order_acquire);
}

int32_t coalesceAfterPeriods() noexcept {
	return utils::state().coalesceAfterPeriods.load(std::memory_order_acquire);
}

std::chrono::milliseconds coalesceMinimumLag() noexcept {
	return std::chrono::milliseconds(utils::state().coalesceMinimumLagMillis.load(std::memory_order_acquire));
}

} // namespace aion::gameserver::runtime::sched_detail

namespace aion::gameserver::utils {

// ------------------------------------------------------------------------------------------------------------------------ ThreadPoolManager

ThreadPoolManager::ThreadPoolManager() = default;

ThreadPoolManager& ThreadPoolManager::getInstance() {
	static auto* instance = new ThreadPoolManager();
	(void)ensureBackend();
	return *instance;
}

void ThreadPoolManager::configure(const Config& config) {
	ManagerState& s = state();
	std::scoped_lock lock(s.mutex);
	if (s.defaultPools)
		throw runtime::IllegalStateException("ThreadPoolManager: the pools already exist, configure() must be called before the first getInstance()");
	s.config = config;
	applyRuntimeValues(s, config);
}

void ThreadPoolManager::installBackend(std::unique_ptr<runtime::ExecutorBackend> backend) {
	ManagerState& s = state();
	std::scoped_lock installLock(s.installMutex);
	for (;;) {
		// 1. retire the published backend while it is still the published one: tasks still running on it (a SerialExecutor hand-over, a
		//    CleanerDrain, any schedule/execute) and hooks that loaded it see a shut-down backend that cancels their submissions; nobody can
		//    observe the replacement state (e.g. no backend) and lazily create the default pools during the swap
		ExecutorBackend* previous = s.backend.load(std::memory_order_acquire);
		if (previous != nullptr)
			previous->retire(); // joins its threads, outside the state mutex
		// 2. publish the replacement, unless the default pools were created lazily meanwhile (then retire those first)
		std::scoped_lock lock(s.mutex);
		if (s.backend.load(std::memory_order_acquire) != previous)
			continue;
		if (s.owned)
			s.retired.push_back(std::move(s.owned)); // 3. never destroyed: readers may still hold it
		s.owned = std::move(backend);
		s.defaultPools = false;
		s.backend.store(s.owned.get(), std::memory_order_release);
		s.shutdown.store(false, std::memory_order_release);
		return;
	}
}

runtime::ExecutorBackend* ThreadPoolManager::installedBackend() noexcept {
	return state().backend.load(std::memory_order_acquire);
}

const runtime::Clock& ThreadPoolManager::clock() noexcept {
	return runtime::sched_detail::schedulerClock();
}

FutureRef ThreadPoolManager::submitToInstalled(runtime::PoolKind pool, Pin pin, Future::Body body, const std::source_location& where,
	const char* kind, bool logExceptions) {
	ExecutorBackend* executor = installedBackend();
	if (executor == nullptr)
		return nullptr;
	Future::Schedule schedule;
	schedule.pool = pool;
	schedule.logExceptions = logExceptions;
	FutureRef future = Future::create(std::move(pin), std::move(body), runtime::TaskInfo{where, kind}, schedule);
	executor->execute(pool, future);
	return future;
}

void ThreadPoolManager::checkPeriod(int64_t period) {
	if (period <= 0)
		throw runtime::IllegalArgumentException(std::format("scheduleAtFixedRate period must be positive: {}", period));
}

FutureRef ThreadPoolManager::submitScheduled(Pin pin, Future::Body body, int64_t delay, TimeUnit unit, int64_t periodMillis,
	const std::source_location& where, const char* kind) {
	if (periodMillis < 0)
		throw runtime::IllegalArgumentException("period < 0");
	ExecutorBackend& executor = ensureBackend();
	Future::Schedule schedule;
	schedule.pool = runtime::PoolKind::SCHEDULED;
	schedule.due = runtime::sched_detail::saturatingAdd(executor.clock().now(), runtime::toNanos(std::max<int64_t>(0, delay), unit));
	schedule.period = std::chrono::milliseconds(periodMillis);
	schedule.logExceptions = true;
	FutureRef future = Future::create(std::move(pin), std::move(body), runtime::TaskInfo{where, kind}, schedule);
	executor.schedule(future);
	return future;
}

FutureRef ThreadPoolManager::submitNow(runtime::PoolKind pool, Pin pin, Future::Body body, const std::source_location& where, const char* kind,
	bool logExceptions) {
	ExecutorBackend& executor = ensureBackend();
	Future::Schedule schedule;
	schedule.pool = pool;
	schedule.logExceptions = logExceptions;
	FutureRef future = Future::create(std::move(pin), std::move(body), runtime::TaskInfo{where, kind}, schedule);
	executor.execute(pool, future);
	return future;
}

std::vector<runtime::TaskInfo> ThreadPoolManager::tasksPinning(const runtime::RefCounted& owner) const {
	std::vector<runtime::TaskInfo> infos;
	for (const FutureRef& task : ensureBackend().pendingTasks())
		if (task->pins(owner))
			infos.push_back(task->getTaskInfo());
	return infos;
}

std::vector<std::string> ThreadPoolManager::getPendingTaskSummary(size_t maxLines) const {
	struct Site {
		size_t count = 0;
		runtime::TaskInfo info;
		bool periodic = false;
	};
	std::map<std::tuple<std::string_view, uint_least32_t, std::string_view, std::string_view>, Site> sites;
	std::vector<FutureRef> tasks = ensureBackend().pendingTasks();
	for (const FutureRef& task : tasks) {
		const runtime::TaskInfo& info = task->getTaskInfo();
		Site& site = sites[{info.where.file_name(), info.where.line(), info.where.function_name(), info.kind}];
		site.info = info;
		site.periodic = site.periodic || task->isPeriodic();
		++site.count;
	}
	std::vector<const Site*> ordered;
	for (const auto& [key, site] : sites)
		ordered.push_back(&site);
	std::stable_sort(ordered.begin(), ordered.end(), [](const Site* a, const Site* b) { return a->count > b->count; });
	std::vector<std::string> lines;
	lines.push_back(std::format("{} pending tasks at {} call sites", tasks.size(), ordered.size()));
	for (size_t i = 0; i < ordered.size() && i < maxLines; ++i)
		lines.push_back(std::format("{} x {}{}", ordered[i]->count, runtime::sched_detail::describeTask(ordered[i]->info),
			ordered[i]->periodic ? " [periodic]" : ""));
	return lines;
}

std::vector<std::string> ThreadPoolManager::getStats() const {
	return ensureBackend().getStats();
}

void ThreadPoolManager::shutdown() {
	ExecutorBackend& executor = ensureBackend();
	int64_t begin = commons::utils::currentTimeMillis();
	std::vector<FutureRef> pending = executor.pendingTasks();
	log().info("ThreadPoolManager: Shutting down.");
	log().info("\t... executing {} scheduled tasks.", countPool(pending, runtime::PoolKind::SCHEDULED));
	log().info("\t... executing {} instant tasks.", countPool(pending, runtime::PoolKind::INSTANT));
	log().info("\t... executing {} long running tasks.", countPool(pending, runtime::PoolKind::LONG_RUNNING));
	pending.clear();

	state().shutdown.store(true, std::memory_order_release);
	bool success = executor.shutdown(std::chrono::milliseconds(5000));

	pending = executor.pendingTasks();
	log().info("\t... success: {} in {} msec.", success, commons::utils::currentTimeMillis() - begin);
	log().info("\t... {} scheduled tasks left.", countPool(pending, runtime::PoolKind::SCHEDULED));
	log().info("\t... {} instant tasks left.", countPool(pending, runtime::PoolKind::INSTANT));
	log().info("\t... {} long running tasks left.", countPool(pending, runtime::PoolKind::LONG_RUNNING));
}

bool ThreadPoolManager::isShutdown() const noexcept {
	return state().shutdown.load(std::memory_order_acquire);
}

runtime::ExecutorBackend& ThreadPoolManager::backend() const noexcept {
	return ensureBackend(); // creation failure inside noexcept terminates (thread creation failed at startup)
}

const ThreadPoolManager::Config& ThreadPoolManager::getConfig() const noexcept {
	return state().config;
}

} // namespace aion::gameserver::utils
