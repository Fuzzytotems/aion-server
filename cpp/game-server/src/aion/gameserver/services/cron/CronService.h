#pragma once

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <typeinfo>
#include <utility>
#include <vector>

#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/runtime/sched/PinnedCallback.h"
#include "aion/gameserver/services/cron/CronExpression.h"

namespace aion::gameserver::services::cron {

/** Java: com.aionemu.gameserver.services.cron.CronServiceException */
class CronServiceException : public runtime::Exception {
public:
	using runtime::Exception::Exception;
};

/** A cron job (Java: Runnable stored in the Quartz JobDataMap). PinnedCallback rules apply (design §7.5, RR-3). */
using CronJob = runtime::PinnedCallback<void()>;

/**
 * Java: com.aionemu.gameserver.services.cron.RunnableRunner - hands a fired job to an executor.
 * Implementations: ThreadPoolManagerRunnableRunner (utils/cron), CurrentThreadRunnableRunner (tests, ShutdownHook restart schedule).
 * Called by the cron driver outside the service's internal lock; exceptions are logged by the service.
 */
class RunnableRunner {
public:
	virtual ~RunnableRunner() = default;
	virtual void executeRunnable(const CronJob& job) = 0;
	virtual void executeLongRunningRunnable(const CronJob& job) = 0;
};

/**
 * Java: com.aionemu.gameserver.services.cron.CurrentThreadRunnableRunner - runs the job on the cron driver's thread (in a TaskScope of kind
 * CRON; nested into the driver's scope). A job that blocks here delays every other cron job, as in Java.
 */
class CurrentThreadRunnableRunner final : public RunnableRunner {
public:
	void executeRunnable(const CronJob& job) override;
	void executeLongRunningRunnable(const CronJob& job) override { executeRunnable(job); }
};

class CronService;

/**
 * Java: org.quartz.JobDetail as returned by CronService.schedule: the handle of a scheduled job. Identity is the handle object.
 * Immutable; thread-safe. Cancelling (CronService::cancel) removes the job from the service; a run already handed to the runner still happens
 * (Quartz deleteJob does not stop a dispatched run either).
 */
class JobDetail final : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND

public:
	const CronJob& getJob() const noexcept { return job_; }
	/** the interned expression (CronExpressions cache, immortal) */
	const CronExpression& getCronExpression() const noexcept { return *expression_; }
	bool isLongRunning() const noexcept { return longRunning_; }
	/** Quartz job key text ("JobKey:Started at ms...; ns...") */
	const std::string& getKey() const noexcept { return key_; }

protected:
	JobDetail(CronJob job, const CronExpression* expression, bool longRunning, std::string key, std::shared_ptr<RunnableRunner> runner,
		uint64_t sequence)
		: job_(std::move(job)), expression_(expression), longRunning_(longRunning), key_(std::move(key)), runner_(std::move(runner)),
		  sequence_(sequence) {}
	~JobDetail() override = default;

private:
	friend class CronService;
	friend struct CronServiceAccess;

	const CronJob job_;
	const CronExpression* const expression_;
	const bool longRunning_;
	const std::string key_;
	/** Java: the RunnableRunner job class of the JobDetail */
	const std::shared_ptr<RunnableRunner> runner_;
	/** service-internal id (scheduling order) */
	const uint64_t sequence_;
};

/**
 * Java: com.aionemu.gameserver.services.cron.CronService (design §1.1, §7.5) with a C++ scheduler instead of Quartz. The service keeps the
 * scheduled jobs ordered by (next fire time, scheduling order) in the service's time zone and hands fired jobs to their RunnableRunner.
 *
 * Drivers (who waits for the next fire time):
 * - THREAD (production): one "Cron" thread sleeps until the earliest fire time (re-checking at least once per second, so wall clock changes
 *   are noticed), then hands the due jobs to the runner inside a TaskScope of kind CRON.
 * - EXECUTOR (gameserver.debug.single_executor, design §1.6, and deterministic tests): no thread; a one-shot task on the ThreadPoolManager's
 *   scheduled pool fires at the earliest fire time and re-arms itself, so cron runs on the single executor thread, and on a
 *   DeterministicExecutor ManualClock::advance fires jobs exactly.
 * - AUTO: EXECUTOR if ThreadPoolManager's Config::singleExecutor is set, else THREAD.
 *
 * Semantics
 * - The first fire time is the first match at or after the scheduling time (whole seconds; Quartz computeFirstFireTime), so a job scheduled
 *   during a matching second fires immediately. A schedule that never fires (years exhausted) throws CronServiceException, like Quartz.
 * - Misfires: due jobs fire once when the driver runs, then the next fire time is computed after the current time (several missed fire times
 *   collapse into one run, Quartz's smart policy for cron triggers). Jobs due at the same time are handed off in (fire time, scheduling
 *   order).
 * - Time: wall clock of the kernel Clock of ThreadPoolManager's backend (ManualClock under DeterministicExecutor). The EXECUTOR driver's delays
 *   run on that backend's steady clock.
 * - Jobs are PinnedCallback<void()> (captures checked like tasks, RR-3); the JobDetail keeps the Pin alive until the job is cancelled or the
 *   service shuts down and the handle is released. A job that triggers shutdown must post to the ShutdownHook thread and return (design §1.4,
 *   deviation 19).
 * - findJobs / findNextFireTimes filter by the stored callable's exact type (std::type_info). Deviation: Java's `withSubTypes` cannot be
 *   evaluated on type-erased C++ callables; ported callers pass the concrete TaskStruct type.
 * - findNextFireTimes: one entry per distinct job (PinnedCallback identity, Java map key = the Runnable) with its earliest next fire time
 *   after now, ordered by fire time.
 * - After shutdown(), schedule and cancel throw CronServiceException (Quartz: scheduler shut down); pending jobs are dropped.
 *
 * Locks: an internal SCHEDULER leaf mutex guards the job table; runners, jobs, logging and ThreadPoolManager calls never run under it. The
 * methods may be called from inside Monitors, compute callbacks and cron jobs (a job may cancel itself, AtreianPassportService.java:47-50),
 * but not while a runtime leaf mutex of rank >= SCHEDULER is held.
 * Thread-safety: all members are thread-safe.
 */
class CronService {
public:
	enum class Driver : uint8_t { AUTO, THREAD, EXECUTOR };

	/** @throws CronServiceException if the service is not initialized (Java getInstance() returns null then) */
	static CronService& getInstance();

	/**
	 * Java initSingleton(runnableRunnerClass, timeZone). `timeZone` nullptr = the system time zone (Java TimeZone null → default).
	 * @throws CronServiceException if already initialized or the runner is null
	 */
	static void initSingleton(std::unique_ptr<RunnableRunner> runnableRunner, const std::chrono::time_zone* timeZone, Driver driver = Driver::AUTO);

	/** Test support: shuts the service down (if initialized) and forgets the singleton, so initSingleton may be called again. */
	static void resetForTests();

	/** Stops the driver; pending jobs are dropped (Java scheduler.shutdown(false)). Safe to call from a job. Idempotent. */
	void shutdown();

	/**
	 * Java schedule(Runnable, String[, longRunning]).
	 * @throws CronServiceException for an invalid expression (cause: CronExpressionParseException), an empty job, a schedule that never fires
	 *         or a shut down service
	 */
	runtime::Ref<JobDetail> schedule(CronJob job, std::string_view cronExpression, bool longRunning = false);
	/** Java schedule(Runnable, CronExpression[, longRunning]); the expression is interned through CronExpressions (its text is the key). */
	runtime::Ref<JobDetail> schedule(CronJob job, const CronExpression& cronExpression, bool longRunning = false);
	/** Java schedule(Runnable, Class<? extends RunnableRunner>, CronExpression, longRunning) (ShutdownHook.java:38). */
	runtime::Ref<JobDetail> schedule(CronJob job, std::shared_ptr<RunnableRunner> runnableRunner, const CronExpression& cronExpression,
		bool longRunning);

	/** @return true if the job was scheduled and is now removed (Java: false for null). @throws CronServiceException after shutdown */
	bool cancel(const JobDetail* jobDetail);
	bool cancel(const runtime::Ref<JobDetail>& jobDetail) { return cancel(jobDetail.get()); }
	/** Cancels every job whose callback is identical to `job` (PinnedCallback identity). @return false if none, else whether all were cancelled */
	bool cancel(const CronJob& job);

	/** jobs whose callback is identical to `job`, in scheduling order */
	std::vector<runtime::Ref<JobDetail>> findJobDetails(const CronJob& job) const;
	/** jobs whose callable type is `type`, in scheduling order */
	std::vector<runtime::Ref<JobDetail>> findJobs(const std::type_info& type) const;
	template <class T>
	std::vector<runtime::Ref<JobDetail>> findJobs() const {
		return findJobs(typeid(T));
	}
	/** earliest next fire time (wall clock, after now) per distinct job of that callable type, ordered by fire time */
	std::vector<std::pair<runtime::Ref<JobDetail>, std::chrono::sys_seconds>> findNextFireTimes(const std::type_info& type) const;
	template <class T>
	std::vector<std::pair<runtime::Ref<JobDetail>, std::chrono::sys_seconds>> findNextFireTimes() const {
		return findNextFireTimes(typeid(T));
	}
	/** Java getJobTriggers(jd).get(0).getNextFireTime(): the stored next fire time, empty if the job is not scheduled */
	std::optional<std::chrono::sys_seconds> getNextFireTime(const JobDetail* jobDetail) const;
	/** number of scheduled jobs */
	size_t getJobCount() const;

	const std::chrono::time_zone* getTimeZone() const noexcept;
	Driver getDriver() const noexcept;

	/**
	 * Fires every job whose fire time has come on the calling thread (hand-off to the runners) and returns their number. Used by both drivers;
	 * public for tests that drive the service by hand.
	 */
	size_t runDueJobs();

private:
	CronService() = default;
	runtime::Ref<JobDetail> scheduleInterned(CronJob job, std::shared_ptr<RunnableRunner> runner, const CronExpression& expression, bool longRunning);
	void rearm();
	friend struct CronServiceAccess;
};

/**
 * Java: com.aionemu.gameserver.services.cron.CronExpressions - process-wide cache of parsed expressions (never freed: configs and jobs keep
 * `const CronExpression*`). Keyed by the exact text. Thread-safe (CONTAINER_SLOT leaf mutex; parsing runs under it).
 */
class CronExpressions {
public:
	/** @throws CronExpressionParseException (Java wraps the ParseException in a RuntimeException) */
	static const CronExpression& getOrCreate(std::string_view cronExpression);
};

} // namespace aion::gameserver::services::cron

namespace aion::commons::configuration::transformers {

template <typename T>
struct PropertyTransformer;

/**
 * Java: com.aionemu.gameserver.services.cron.CronExpressionTransformer - config fields of type CronExpression become
 * `const CronExpression*` (nullptr for an empty value, Java null), resolved through CronExpressions::getOrCreate.
 */
template <>
struct PropertyTransformer<const gameserver::services::cron::CronExpression*> {
	static std::string typeName() { return "CronExpression"; }
	static const gameserver::services::cron::CronExpression* parseObject(std::string_view value) {
		return value.empty() ? nullptr : &gameserver::services::cron::CronExpressions::getOrCreate(value);
	}
};

} // namespace aion::commons::configuration::transformers
