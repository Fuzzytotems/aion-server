#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <typeinfo>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/runtime/collections/ConcurrentLinkedQueue.h"
#include "aion/gameserver/runtime/collections/HashSet.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/runtime/sync/Monitor.h"
#include "aion/gameserver/taskmanager/AbstractPeriodicTaskManager.h"
#include "aion/gameserver/taskmanager/fwd.h"

namespace aion::gameserver::taskmanager {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze. A class
 * template (the unbounded generic stays a template, §8.1); the queued tasks are Refs (the task classes are RefCounted). The logger and
 * statistics calls of run() go through the non-template fifo* helpers of AbstractPeriodicTaskManager. Java's `getClass().getSimpleName()` is
 * the name the subclass passes to the two-argument constructor: every subclass must use it (the one-argument constructor logs
 * "AbstractFIFOPeriodicTaskManager"; docs/deviations/P4-10.md).
 * <p>
 * The members that need the complete task class T (run, taskClassOf, describeTask, the destructor) are defined below the class, not inline.
 * A subclass header over a class it may not include (another hub, hub-headers.md §3.1) declares
 * `extern template class AbstractFIFOPeriodicTaskManager<T>;` with T forward-declared, and exactly one .cpp that includes T's header holds
 * the explicit instantiation `template class AbstractFIFOPeriodicTaskManager<T>;` (for Creature: AbstractPeriodicTaskManager.cpp). Without
 * the extern declaration the members are instantiated implicitly where T is complete (BrokerService.cpp).
 *
 * @author lord_rex and MrPoke (based on l2j-free engines), Neon
 */
template <class T>
class AbstractFIFOPeriodicTaskManager : public AbstractPeriodicTaskManager {
	AION_MAKE_REF_FRIEND
private:
	static constexpr int32_t WARNING_PERIOD_SECONDS = 10;
	// Java: = new ConcurrentLinkedQueue<>()
	runtime::ConcurrentLinkedQueue<runtime::Ref<T>> tasks{AION_LOCK_CLASS(AbstractFIFOPeriodicTaskManager::tasks)};
	// Java: = new LinkedHashSet<>()
	runtime::LinkedHashSet<runtime::Ref<T>> processedTasks{AION_LOCK_CLASS(AbstractFIFOPeriodicTaskManager::processedTasks)};
	const int32_t counterLimit;
	runtime::Field<int32_t> counter{0};
	// fieldmap.toml: C++-only immutable copy of Java getClass().getSimpleName() of the subclass (AbstractPeriodicTaskManager class comment)
	const std::string simpleClassName;

protected:
	explicit AbstractFIFOPeriodicTaskManager(int32_t periodMillis)
		: AbstractPeriodicTaskManager(periodMillis), counterLimit(std::max(5, WARNING_PERIOD_SECONDS * 1000 / periodMillis)),
		  simpleClassName("AbstractFIFOPeriodicTaskManager") {}

	/** C++ only: the Java constructor with the subclass's simple class name */
	AbstractFIFOPeriodicTaskManager(int32_t periodMillis, std::string_view simpleClassNameValue)
		: AbstractPeriodicTaskManager(periodMillis, simpleClassNameValue), counterLimit(std::max(5, WARNING_PERIOD_SECONDS * 1000 / periodMillis)),
		  simpleClassName(simpleClassNameValue) {}

public:
	/** Java final */
	void add(T& t) { tasks.add(runtime::Ref<T>(t)); }

	// synchronized
	void run() override final;

private:
	/** Java: task.getClass() */
	static const std::type_info& taskClassOf(T& task);

	/** Java: String.valueOf(task) - toString() where the task class declares one, otherwise the class name (Java Object.toString without the hash) */
	static std::string describeTask(T& task);

protected:
	virtual void callTask(T& task) = 0;

	virtual std::string getCalledMethodName() = 0;

	~AbstractFIFOPeriodicTaskManager() override;
};

// ---- members that need the complete task class (class comment) ------------------------------------------------------------------------------

template <class T>
void AbstractFIFOPeriodicTaskManager<T>::run() {
	SYNCHRONIZED(*this) {
		int32_t previouslyProcessedTasksSize = processedTasks.size();
		processedTasks.clear();
		for (int32_t i = tasks.size(); i > 0; --i) {
			runtime::Ptr<T> task = tasks.poll();
			if (!task) // no tasks left
				break;
			processedTasks.add(runtime::Ref<T>(task));
		}
		for (runtime::Ptr<T> task : processedTasks.snapshot()) {
			try {
				int64_t begin = commons::utils::nanoTime();
				callTask(*task);
				if (fifoStatsEnabled()) {
					int64_t duration = commons::utils::nanoTime() - begin;
					fifoHandleStats(taskClassOf(*task), getCalledMethodName(), duration);
				}
			} catch (...) {
				fifoLogTaskException(simpleClassName, describeTask(*task));
			}
		}
		if (processedTasks.size() <= previouslyProcessedTasksSize)
			counter.set(0);
		else if ((counter += 1) % counterLimit == 0) // log warning if the task queue size continually increased over the last WARNING_PERIOD_SECONDS
			fifoLogTasksAddedFaster(simpleClassName, processedTasks.size());
	}
}

template <class T>
const std::type_info& AbstractFIFOPeriodicTaskManager<T>::taskClassOf(T& task) {
	return typeid(task);
}

template <class T>
std::string AbstractFIFOPeriodicTaskManager<T>::describeTask(T& task) {
	if constexpr (requires { task.toString(); })
		return std::string(task.toString());
	else
		return fifoSimpleClassName(typeid(task));
}

template <class T>
AbstractFIFOPeriodicTaskManager<T>::~AbstractFIFOPeriodicTaskManager() = default;

} // namespace aion::gameserver::taskmanager
