#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/taskmanager/AbstractPeriodicTaskManager.h"

namespace aion::gameserver::taskmanager::tasks {

/**
 * Every 4 s: the protectors (legion dominion rank 3) inside an occupied Stonespear Reach location's zone are told the conquerors inside it.
 * <p>
 * java-dead: nothing in Java calls getInstance(), so the task is never created or scheduled; ported for completeness (m5i-plan.md LD-01, D14).
 * C++: RefCounted through AbstractPeriodicTaskManager; the singleton is a never-released Ref (hub-headers.md §11.1). M5i I-9, under P5-12's
 * lease.
 */
class LegionDominionIntruderUpdateTask : public AbstractPeriodicTaskManager {
	AION_MAKE_REF_FRIEND
protected:
	LegionDominionIntruderUpdateTask();
	~LegionDominionIntruderUpdateTask() override;

public:
	static LegionDominionIntruderUpdateTask& getInstance();

	void run() override;
};

} // namespace aion::gameserver::taskmanager::tasks
