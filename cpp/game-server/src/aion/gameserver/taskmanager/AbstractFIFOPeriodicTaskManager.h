#pragma once

#include <algorithm>
#include <cstdint>
#include <string>

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/collections/ConcurrentLinkedQueue.h"
#include "aion/gameserver/runtime/collections/HashSet.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/taskmanager/AbstractPeriodicTaskManager.h"
#include "aion/gameserver/taskmanager/fwd.h"

namespace aion::gameserver::taskmanager {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze. A class
 * template (the unbounded generic stays a template, §8.1) with the member bodies inline; the queued tasks are Refs (the task classes are
 * RefCounted).
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

protected:
	explicit AbstractFIFOPeriodicTaskManager(int32_t periodMillis)
		: AbstractPeriodicTaskManager(periodMillis), counterLimit(std::max(5, WARNING_PERIOD_SECONDS * 1000 / periodMillis)) {}

public:
	/** Java final */
	void add(T& t) { AION_UNPORTED(); }

	// synchronized
	void run() override final { AION_UNPORTED(); }

protected:
	virtual void callTask(T& task) = 0;

	virtual std::string getCalledMethodName() = 0;

	~AbstractFIFOPeriodicTaskManager() override = default;
};

} // namespace aion::gameserver::taskmanager
