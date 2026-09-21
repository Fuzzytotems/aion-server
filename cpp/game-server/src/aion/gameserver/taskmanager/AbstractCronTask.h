#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "aion/commons/database/SqlTypes.h"
#include "aion/commons/logging/Logger.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/runtime/sync/Semaphore.h"
#include "aion/gameserver/services/cron/CronExpression.h"
#include "aion/gameserver/taskmanager/fwd.h"

namespace aion::gameserver::taskmanager {

/**
 * Base of cron tasks that run on a (configurable) cron expression and catch up on a run the server missed while it was down (housing auction
 * end, auction auto fill, maintenance).
 * <p>
 * C++ notes:
 * - Two-phase construction (docs/deviations/P5-14.md): Java's constructor ends with runAndScheduleAsyncWithLock, whose task calls the virtual
 *   shouldRunOnStart() and executeTask() on another thread, possibly before the subclass constructor has finished. A C++ object must not be
 *   used through its subclass before that constructor returns, so the constructor only computes the fields, and the subclass's create() (or
 *   getInstance) calls postConstruct() right after construction, which acquires the semaphore and hands the task to the long running pool
 *   (pinning this task). postConstruct() does nothing for a deactivated task (null cron expression).
 * - Java's `getClass()` logger and simple name: the subclass passes its Java class name (fully qualified) to the constructor.
 * - SERVER_STOP_MILLIS is read from ServerVariablesDAO when the class is initialized in Java; the static serverStopMillis() reads it on its
 *   first call and caches it (hub-headers.md §11.1: no static initializer may reach the database).
 * - The semaphore is released even if the start run throws (Java would keep it and block the next cron task's constructor forever;
 *   docs/deviations/P5-14.md).
 * - Dates are commons::database::Timestamp (§6), null dates std::optional; the cron expression is the interned `const CronExpression*` of the
 *   config (null = deactivated). Cron times are computed in the configured game server time zone (GSConfig.TIME_ZONE_ID, as CronService).
 *
 * @author Rolandas, Neon
 */
class AbstractCronTask : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
protected:
	/** Java: protected static final Long SERVER_STOP_MILLIS = ServerVariablesDAO.loadLong("serverLastRun") (cached on first use, class comment) */
	static std::optional<int64_t> serverStopMillis();

private:
	static runtime::Semaphore semaphore; // Java: = new Semaphore(1)

protected:
	/** Java: LoggerFactory.getLogger(getClass()) */
	const commons::logging::Logger log;

private:
	/** C++ only: Java getClass().getSimpleName() of the subclass */
	const std::string simpleClassName;
	// fieldmap: Java's CronExpression field is null for a deactivated task (the constructor checks it), so it is the interned pointer of the config
	const services::cron::CronExpression* const cronExpression;
	// fieldmap: the three Dates are null until they are computed (getLastRun, getLastPlannedRun and getNextRun return null in Java), so they are optional (§6)
	const std::optional<commons::database::Timestamp> lastPlannedRunBeforeServerStart;
	// fieldmap: see lastPlannedRunBeforeServerStart
	runtime::Field<std::optional<commons::database::Timestamp>> lastRun{};
	// fieldmap: see lastPlannedRunBeforeServerStart
	runtime::Field<std::optional<commons::database::Timestamp>> nextRun{};

public:
	/**
	 * @param cronExpression
	 *          null deactivates the task
	 * @param className
	 *          C++ only: the Java class name of the subclass (logger name; its last segment is the simple name)
	 */
	AbstractCronTask(const services::cron::CronExpression* cronExpression, std::string_view className);

protected:
	~AbstractCronTask() override;

	/** C++ only: the tail of the Java constructor (runAndScheduleAsyncWithLock), called by the subclass's create() (class comment) */
	void postConstruct();

private:
	/**
	 * Runs this task in another thread, so that constructor can finish and external references to this task don't throw null pointer exceptions.
	 * This additionally makes sure that other tasks don't run before a previous one is finished, so multiple tasks are initialized
	 * semi-synchronous.
	 */
	void runAndScheduleAsyncWithLock();

protected:
	/** @return Default implementation returns true if the server was down when task should have run */
	virtual bool shouldRunOnStart();

public:
	/** @return The last time this task started, null if it didn't during this uptime yet */
	std::optional<commons::database::Timestamp> getLastRun() const;

	/** @return The last time this task started or should have started (in case task hasn't yet run since the server got restarted) */
	std::optional<commons::database::Timestamp> getLastPlannedRun() const;

	int64_t getMillisSinceLastRun() const;

	/** @return Time of the next task start */
	std::optional<commons::database::Timestamp> getNextRun() const;

	/** @return Time of the next task start after given date */
	std::optional<commons::database::Timestamp> getNextRunAfter(commons::database::Timestamp date) const;

	int64_t getMillisUntilNextRun() const;

protected:
	virtual void executeTask() = 0;

public:
	/** Java final (Runnable.run) */
	void run();

private:
	/**
	 * @return Date when this task last should have run, whether the server was online or not. <b>NOTE</b>: The current implementation may not find
	 *         the correct date if the underlying cron expression is irregular
	 */
	std::optional<commons::database::Timestamp> findLastPlannedRun(commons::database::Timestamp nextRunValue) const;

	/** C++ only: the constructor's computation of lastPlannedRunBeforeServerStart (null for a deactivated task) */
	std::optional<commons::database::Timestamp> initialLastPlannedRun() const;
};

} // namespace aion::gameserver::taskmanager
