#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <typeinfo>

#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/taskmanager/fwd.h"

namespace aion::commons::logging {
class Logger;
} // namespace aion::commons::logging

namespace aion::gameserver::taskmanager {

/**
 * This can be used for periodic calls.
 * <p>
 * S0c declaration header (docs/design/hub-headers.md §3.5). The constructor logs and schedules `this::run` at a fixed rate (the method
 * reference at AbstractPeriodicTaskManager.java:18, pinned `{this}`); the first run comes 500-550 ms later, after the subclass is constructed.
 * <p>
 * C++ additions: Java logs `getClass().getSimpleName()`, which a C++ base constructor cannot see (typeid(*this) is still the base), so every
 * subclass must pass its simple class name to the two-argument constructor; the one-argument constructor logs the name of this class (a porting
 * rule for the remaining subclasses: ExpireTimerTask, LegionDominionIntruderUpdateTask, TemporaryTradeTimeTask, TeamMoveUpdater,
 * TeamStatUpdater; docs/deviations/P4-10.md). The static fifo* helpers are the non-template parts of AbstractFIFOPeriodicTaskManager::run
 * (logger and statistics), defined in the .cpp.
 * <p>
 * Accepted risk (as in Java, docs/deviations/P4-10.md): the constructor hands `this` to the scheduler before the subclass constructor has run.
 * The first run comes 500-550 ms later; a subclass constructor that takes that long (a stalled startup thread, a debugger) would see run() called
 * on an object that is still being constructed.
 *
 * @author lord_rex and MrPoke based on l2j-free engines
 */
class AbstractPeriodicTaskManager : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
protected:
	/** Java: protected static final Logger log = LoggerFactory.getLogger(AbstractPeriodicTaskManager.class) */
	static const commons::logging::Logger log;

	/** Logs "AbstractPeriodicTaskManager initialized."; subclasses use the two-argument constructor instead (class comment) */
	explicit AbstractPeriodicTaskManager(int32_t period);

	/** C++ only: the Java constructor with the subclass's getClass().getSimpleName() for the log line (class comment) */
	AbstractPeriodicTaskManager(int32_t period, std::string_view simpleClassName);

	virtual void run() = 0;

	~AbstractPeriodicTaskManager() override;

	/** C++ only: Java `log.error("Exception in " + getClass().getSimpleName() + " processing " + task, e)` with the current exception */
	static void fifoLogTaskException(std::string_view simpleClassName, const std::string& task);

	/** C++ only: Java RunnableStatsManager.handleStats(task.getClass(), getCalledMethodName(), duration) if CommonsConfig.RUNNABLESTATS_ENABLE */
	static void fifoHandleStats(const std::type_info& taskClass, std::string_view calledMethodName, int64_t duration);

	/** C++ only: CommonsConfig.RUNNABLESTATS_ENABLE */
	static bool fifoStatsEnabled();

	/** C++ only: utils::simpleClassName(type) for tasks without toString() */
	static std::string fifoSimpleClassName(const std::type_info& type);

	/** C++ only: Java's warning that the tasks are added faster than they can be executed */
	static void fifoLogTasksAddedFaster(std::string_view simpleClassName, int32_t size);
};

} // namespace aion::gameserver::taskmanager
