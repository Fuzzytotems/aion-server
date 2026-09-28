// CronService, CronExpressions and CurrentThreadRunnableRunner (design §1.1, §7.5). See CronService.h for the semantics.

#include "aion/gameserver/services/cron/CronService.h"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <format>
#include <map>
#include <set>
#include <thread>
#include <unordered_map>

#include "aion/commons/configuration/transformers/ZoneIdTransformer.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/commons/utils/concurrent/ThreadName.h"
#include "aion/gameserver/runtime/base/ThreadContext.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sync/RankedMutex.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

namespace aion::gameserver::services::cron {

using runtime::FutureRef;
using runtime::LockRank;
using runtime::Ref;
using std::chrono::sys_seconds;

namespace {

const commons::logging::Logger& log() {
	static const auto* instance = new commons::logging::Logger(commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.cron.CronService"));
	return *instance;
}

/** wall clock of the scheduler (ManualClock under DeterministicExecutor); never called under the service lock */
int64_t currentTimeMillis() {
	return utils::ThreadPoolManager::clock().currentTimeMillis(); // never creates the default pools
}

sys_seconds toSeconds(int64_t millis) noexcept {
	int64_t secondsValue = millis / 1000;
	if (millis % 1000 < 0)
		--secondsValue;
	return sys_seconds(std::chrono::seconds(secondsValue));
}

int64_t toMillis(sys_seconds time) noexcept {
	return time.time_since_epoch().count() * 1000;
}

struct Scheduled {
	Ref<JobDetail> detail;
	sys_seconds nextFire;
};

/** Process-wide service state (leaked, like the other kernel singletons). Guarded by `mutex` unless noted. */
struct CronState {
	runtime::RankedMutex<LockRank::SCHEDULER> mutex;
	std::condition_variable_any wake;
	/** lock-free reads (getInstance, getTimeZone, getDriver) */
	std::atomic<bool> initialized{false};
	std::atomic<const std::chrono::time_zone*> zone{nullptr};
	std::atomic<CronService::Driver> driver{CronService::Driver::THREAD};
	bool shutdown = false;
	std::shared_ptr<RunnableRunner> runner;
	uint64_t nextSequence = 1;
	/** by scheduling order */
	std::map<uint64_t, Scheduled> jobs;
	/** (next fire time, sequence) */
	std::set<std::pair<sys_seconds, uint64_t>> queue;
	/** incremented on every change of the queue (THREAD driver wake-up predicate) */
	uint64_t changes = 0;
	// THREAD driver
	std::thread thread;
	/** a THREAD driver thread runs while this equals the generation it was started with (shutdown increments it) */
	uint64_t threadGeneration = 0;
	// EXECUTOR driver
	FutureRef tick;
	std::optional<sys_seconds> tickFor;
	uint64_t tickGeneration = 0;
};

CronState& state() {
	static auto* instance = new CronState();
	return *instance;
}

CronServiceException shutDownException(const char* what) {
	return CronServiceException(what, std::make_exception_ptr(runtime::IllegalStateException("The Scheduler has been shutdown.")));
}

} // namespace

/** Driver glue with access to JobDetail internals. */
struct CronServiceAccess {
	/** EXECUTOR driver: the captureless tick task */
	static void tick();
	/** fires due jobs; a nonzero `threadGeneration` fires nothing if that THREAD driver thread is stale */
	static size_t fireDue(uint64_t threadGeneration);
};

namespace {

void threadMain(uint64_t generation) {
	commons::utils::concurrent::setCurrentThreadName("Cron");
	runtime::ThreadContext::current().refreshName();
	CronState& s = state();
	for (;;) {
		try {
			(void)CronServiceAccess::fireDue(generation);
		} catch (...) {
			log().errorCurrentException("Cron thread: failed to run due jobs");
		}
		int64_t nowMillis = 0;
		try {
			nowMillis = currentTimeMillis();
		} catch (...) {
			log().errorCurrentException("Cron thread: no clock");
		}
		std::unique_lock lock(s.mutex);
		if (s.threadGeneration != generation)
			return;
		auto wait = std::chrono::milliseconds(1000);
		if (!s.queue.empty())
			wait = std::chrono::milliseconds(std::clamp<int64_t>(toMillis(s.queue.begin()->first) - nowMillis, 0, 1000));
		if (wait.count() > 0) {
			uint64_t seen = s.changes;
			s.wake.wait_for(lock, wait, [&] { return s.threadGeneration != generation || s.changes != seen; });
		}
		if (s.threadGeneration != generation)
			return;
	}
}

} // namespace

void CronServiceAccess::tick() {
	CronState& s = state();
	if (!s.initialized.load(std::memory_order_acquire))
		return;
	{
		std::scoped_lock lock(s.mutex);
		s.tickFor.reset(); // this tick is consumed: rearm() must schedule the next one even for the same fire time
	}
	CronService& service = CronService::getInstance();
	(void)fireDue(0);
	service.rearm();
}

// ---------------------------------------------------------------------------------------------------------------------- runners and cache

void CurrentThreadRunnableRunner::executeRunnable(const CronJob& job) {
	runtime::TaskScope scope(runtime::TaskInfo{job.getTaskInfo().where, runtime::TaskKind::CRON});
	job();
}

namespace {

struct TransparentHash {
	using is_transparent = void;
	size_t operator()(std::string_view text) const noexcept { return std::hash<std::string_view>{}(text); }
};

struct ExpressionCache {
	runtime::RankedMutex<LockRank::CONTAINER_SLOT> mutex;
	std::unordered_map<std::string, std::unique_ptr<const CronExpression>, TransparentHash, std::equal_to<>> expressions;
};

ExpressionCache& expressionCache() {
	static auto* instance = new ExpressionCache();
	return *instance;
}

} // namespace

const CronExpression& CronExpressions::getOrCreate(std::string_view cronExpression) {
	ExpressionCache& cache = expressionCache();
	{
		std::scoped_lock lock(cache.mutex);
		if (auto it = cache.expressions.find(cronExpression); it != cache.expressions.end())
			return *it->second;
	}
	auto parsed = std::make_unique<const CronExpression>(cronExpression); // parse outside the leaf mutex; throws for invalid text
	std::scoped_lock lock(cache.mutex);
	auto [it, inserted] = cache.expressions.try_emplace(std::string(cronExpression), std::move(parsed));
	return *it->second;
}

// ---------------------------------------------------------------------------------------------------------------------------- CronService

CronService& CronService::getInstance() {
	static auto* instance = new CronService();
	if (!state().initialized.load(std::memory_order_acquire))
		throw CronServiceException("CronService is not initialized");
	return *instance;
}

void CronService::initSingleton(std::unique_ptr<RunnableRunner> runnableRunner, const std::chrono::time_zone* timeZone, Driver driver) {
	CronState& s = state();
	if (s.initialized.load(std::memory_order_acquire))
		throw CronServiceException("CronService is already initialized");
	if (!runnableRunner)
		throw CronServiceException("RunnableRunner class must be defined");
	if (timeZone == nullptr)
		timeZone = commons::configuration::transformers::ZoneIdTransformer::systemDefault();
	if (driver == Driver::AUTO)
		driver = utils::ThreadPoolManager::getInstance().getConfig().singleExecutor ? Driver::EXECUTOR : Driver::THREAD;

	std::scoped_lock lock(s.mutex);
	if (s.initialized.load(std::memory_order_acquire))
		throw CronServiceException("CronService is already initialized");
	s.runner = std::move(runnableRunner);
	s.zone.store(timeZone, std::memory_order_release);
	s.driver.store(driver, std::memory_order_release);
	s.shutdown = false;
	s.tickFor.reset();
	s.initialized.store(true, std::memory_order_release);
	if (driver == Driver::THREAD) {
		try {
			s.thread = std::thread(threadMain, ++s.threadGeneration); // its first runDueJobs waits for this lock, i.e. until initialization is complete
		} catch (...) {
			s.initialized.store(false, std::memory_order_release);
			throw;
		}
	}
}

void CronService::resetForTests() {
	CronState& s = state();
	if (!s.initialized.load(std::memory_order_acquire))
		return;
	getInstance().shutdown();
	std::shared_ptr<RunnableRunner> runner;
	{
		std::scoped_lock lock(s.mutex);
		runner = std::move(s.runner);
		s.initialized.store(false, std::memory_order_release);
		s.shutdown = false;
		s.tickFor.reset();
	}
}

void CronService::shutdown() {
	CronState& s = state();
	std::thread thread;
	FutureRef tick;
	std::map<uint64_t, Scheduled> dropped;
	{
		std::scoped_lock lock(s.mutex);
		if (s.shutdown)
			return;
		s.shutdown = true;
		++s.threadGeneration;
		thread = std::move(s.thread);
		tick = std::move(s.tick);
		s.tickFor.reset();
		dropped.swap(s.jobs);
		s.queue.clear();
		++s.changes;
	}
	s.wake.notify_all();
	if (tick)
		tick->cancel();
	if (thread.joinable()) {
		if (thread.get_id() == std::this_thread::get_id())
			thread.detach(); // shutdown from a job run by CurrentThreadRunnableRunner on the cron thread: the loop ends after the job
		else
			thread.join();
	}
}

Ref<JobDetail> CronService::schedule(CronJob job, std::string_view cronExpression, bool longRunning) {
	const CronExpression* expression = nullptr;
	try {
		expression = &CronExpressions::getOrCreate(cronExpression);
	} catch (...) {
		throw CronServiceException("Failed to start job", std::current_exception());
	}
	std::shared_ptr<RunnableRunner> runner;
	{
		CronState& s = state();
		std::scoped_lock lock(s.mutex);
		runner = s.runner;
	}
	return scheduleInterned(std::move(job), std::move(runner), *expression, longRunning);
}

Ref<JobDetail> CronService::schedule(CronJob job, const CronExpression& cronExpression, bool longRunning) {
	std::shared_ptr<RunnableRunner> runner;
	{
		CronState& s = state();
		std::scoped_lock lock(s.mutex);
		runner = s.runner;
	}
	return schedule(std::move(job), std::move(runner), cronExpression, longRunning);
}

Ref<JobDetail> CronService::schedule(CronJob job, std::shared_ptr<RunnableRunner> runnableRunner, const CronExpression& cronExpression,
	bool longRunning) {
	// callers may pass expressions with any lifetime; the job keeps the immortal interned copy (same text, same parse result)
	const CronExpression& interned = CronExpressions::getOrCreate(cronExpression.getCronExpression());
	return scheduleInterned(std::move(job), std::move(runnableRunner), interned, longRunning);
}

Ref<JobDetail> CronService::scheduleInterned(CronJob job, std::shared_ptr<RunnableRunner> runner, const CronExpression& expression, bool longRunning) {
	CronState& s = state();
	if (!job)
		throw CronServiceException("Failed to start job", std::make_exception_ptr(runtime::NullPointerException("the job is null")));
	if (!runner)
		throw CronServiceException("Failed to start job", std::make_exception_ptr(runtime::IllegalArgumentException("RunnableRunner class must be defined")));
	int64_t nowMillis = currentTimeMillis();
	const std::chrono::time_zone* zone = s.zone.load(std::memory_order_acquire);
	if (zone == nullptr)
		throw CronServiceException("CronService is not initialized");
	// Quartz CronTriggerImpl.computeFirstFireTime: getFireTimeAfter(startTime - 1 s)
	std::optional<sys_seconds> first = expression.getNextValidTimeAfter(toSeconds(nowMillis) - std::chrono::seconds(1), zone);
	if (!first)
		throw CronServiceException("Failed to start job",
			std::make_exception_ptr(runtime::IllegalStateException("Based on configured schedule, the given trigger will never fire.")));
	std::string key = std::format("JobKey:Started at ms{}; ns{}", commons::utils::currentTimeMillis(), commons::utils::nanoTime());

	uint64_t sequence = 0;
	{
		std::scoped_lock lock(s.mutex);
		if (!s.initialized.load(std::memory_order_acquire) || s.shutdown)
			throw shutDownException("Failed to start job");
		sequence = s.nextSequence++;
	}
	Ref<JobDetail> detail = runtime::makeRef<JobDetail>(std::move(job), &expression, longRunning, std::move(key), std::move(runner), sequence);
	bool earliest = false;
	{
		std::scoped_lock lock(s.mutex);
		if (!s.initialized.load(std::memory_order_acquire) || s.shutdown)
			throw shutDownException("Failed to start job");
		s.jobs.emplace(sequence, Scheduled{detail, *first});
		s.queue.emplace(*first, sequence);
		earliest = s.queue.begin()->second == sequence;
		++s.changes;
	}
	if (earliest) {
		s.wake.notify_all();
		if (s.driver.load(std::memory_order_acquire) == Driver::EXECUTOR)
			rearm();
	}
	return detail;
}

bool CronService::cancel(const JobDetail* jobDetail) {
	if (jobDetail == nullptr)
		return false;
	CronState& s = state();
	Ref<JobDetail> removed;
	bool earliest = false;
	{
		std::scoped_lock lock(s.mutex);
		if (!s.initialized.load(std::memory_order_acquire) || s.shutdown)
			throw shutDownException("Failed to delete Job");
		auto it = s.jobs.find(jobDetail->sequence_);
		if (it == s.jobs.end() || it->second.detail.get() != jobDetail)
			return false;
		earliest = s.queue.begin()->second == it->first;
		s.queue.erase({it->second.nextFire, it->first});
		removed = std::move(it->second.detail);
		s.jobs.erase(it);
		++s.changes;
	}
	if (earliest) {
		s.wake.notify_all();
		if (s.driver.load(std::memory_order_acquire) == Driver::EXECUTOR)
			rearm();
	}
	return true;
}

bool CronService::cancel(const CronJob& job) {
	std::vector<Ref<JobDetail>> details = findJobDetails(job);
	if (details.empty())
		return false;
	bool allCancelled = true;
	for (const Ref<JobDetail>& detail : details)
		allCancelled &= cancel(detail.get());
	return allCancelled;
}

std::vector<Ref<JobDetail>> CronService::findJobDetails(const CronJob& job) const {
	CronState& s = state();
	std::vector<Ref<JobDetail>> result;
	std::scoped_lock lock(s.mutex);
	for (const auto& [sequence, scheduled] : s.jobs)
		if (scheduled.detail->job_ == job)
			result.push_back(scheduled.detail);
	return result;
}

std::vector<Ref<JobDetail>> CronService::findJobs(const std::type_info& type) const {
	CronState& s = state();
	std::vector<Ref<JobDetail>> result;
	std::scoped_lock lock(s.mutex);
	for (const auto& [sequence, scheduled] : s.jobs)
		if (scheduled.detail->job_.target_type() == type)
			result.push_back(scheduled.detail);
	return result;
}

std::vector<std::pair<Ref<JobDetail>, sys_seconds>> CronService::findNextFireTimes(const std::type_info& type) const {
	CronState& s = state();
	sys_seconds now = toSeconds(currentTimeMillis());
	std::vector<std::pair<Ref<JobDetail>, sys_seconds>> result;
	{
		std::scoped_lock lock(s.mutex);
		for (const auto& [sequence, scheduled] : s.jobs) {
			if (scheduled.detail->job_.target_type() != type || scheduled.nextFire <= now)
				continue;
			auto same = std::find_if(result.begin(), result.end(), [&](const auto& entry) { return entry.first->job_ == scheduled.detail->job_; });
			if (same == result.end())
				result.emplace_back(scheduled.detail, scheduled.nextFire);
			else if (scheduled.nextFire < same->second)
				*same = {scheduled.detail, scheduled.nextFire};
		}
	}
	std::stable_sort(result.begin(), result.end(), [](const auto& a, const auto& b) { return a.second < b.second; });
	return result;
}

std::optional<sys_seconds> CronService::getNextFireTime(const JobDetail* jobDetail) const {
	if (jobDetail == nullptr)
		return std::nullopt;
	CronState& s = state();
	std::scoped_lock lock(s.mutex);
	auto it = s.jobs.find(jobDetail->sequence_);
	if (it == s.jobs.end() || it->second.detail.get() != jobDetail)
		return std::nullopt;
	return it->second.nextFire;
}

size_t CronService::getJobCount() const {
	CronState& s = state();
	std::scoped_lock lock(s.mutex);
	return s.jobs.size();
}

const std::chrono::time_zone* CronService::getTimeZone() const noexcept {
	return state().zone.load(std::memory_order_acquire);
}

CronService::Driver CronService::getDriver() const noexcept {
	return state().driver.load(std::memory_order_acquire);
}

size_t CronService::runDueJobs() {
	return CronServiceAccess::fireDue(0);
}

size_t CronServiceAccess::fireDue(uint64_t threadGeneration) {
	CronState& s = state();
	int64_t nowMillis = currentTimeMillis();
	sys_seconds now = toSeconds(nowMillis);
	std::vector<Ref<JobDetail>> due;
	std::vector<Ref<JobDetail>> finished;
	{
		std::scoped_lock lock(s.mutex);
		if (!s.initialized.load(std::memory_order_acquire) || s.shutdown || (threadGeneration != 0 && s.threadGeneration != threadGeneration))
			return 0;
		const std::chrono::time_zone* zone = s.zone.load(std::memory_order_acquire);
		while (!s.queue.empty() && s.queue.begin()->first <= now) {
			uint64_t sequence = s.queue.begin()->second;
			s.queue.erase(s.queue.begin());
			auto it = s.jobs.find(sequence);
			due.push_back(it->second.detail);
			// misfires collapse: the next fire time is computed after the current time, not after the missed fire time
			std::optional<sys_seconds> next = it->second.detail->expression_->getNextValidTimeAfter(now, zone);
			if (next) {
				it->second.nextFire = *next;
				s.queue.emplace(*next, sequence);
			} else {
				finished.push_back(std::move(it->second.detail));
				s.jobs.erase(it);
			}
			++s.changes;
		}
	}
	if (due.empty())
		return 0;
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::CRON));
	for (const Ref<JobDetail>& detail : due) {
		try {
			if (detail->longRunning_)
				detail->runner_->executeLongRunningRunnable(detail->job_);
			else
				detail->runner_->executeRunnable(detail->job_);
		} catch (...) {
			log().errorCurrentException(std::format("Failed to execute cron job {} ({})", detail->key_, detail->expression_->getCronExpression()));
		}
	}
	return due.size();
}

void CronService::rearm() {
	CronState& s = state();
	FutureRef previous;
	std::optional<sys_seconds> next;
	uint64_t generation = 0;
	{
		std::scoped_lock lock(s.mutex);
		if (!s.initialized.load(std::memory_order_acquire) || s.shutdown || s.driver.load(std::memory_order_acquire) != Driver::EXECUTOR) {
			previous = std::move(s.tick);
			s.tickFor.reset();
		} else {
			if (!s.queue.empty())
				next = s.queue.begin()->first;
			if (next == s.tickFor && (!next || (s.tick && !s.tick->isDone())))
				return;
			previous = std::move(s.tick);
			s.tickFor = next;
			generation = ++s.tickGeneration;
		}
	}
	if (previous)
		previous->cancel();
	if (!next)
		return;
	int64_t delay = std::max<int64_t>(0, toMillis(*next) - currentTimeMillis());
	FutureRef task = utils::ThreadPoolManager::getInstance().schedule([] { CronServiceAccess::tick(); }, delay);
	FutureRef stale;
	{
		std::scoped_lock lock(s.mutex);
		if (s.tickGeneration == generation && !s.shutdown && s.initialized.load(std::memory_order_acquire))
			s.tick = std::move(task);
		else
			stale = std::move(task);
	}
	if (stale)
		stale->cancel();
}

} // namespace aion::gameserver::services::cron
