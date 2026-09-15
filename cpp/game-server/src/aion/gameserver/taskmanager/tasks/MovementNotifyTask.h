#pragma once

#include <string>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/taskmanager/AbstractFIFOPeriodicTaskManager.h"
#include "aion/gameserver/taskmanager/tasks/fwd.h"

// instantiated in AbstractPeriodicTaskManager.cpp, so this header needs no complete Creature (hub-headers.md §3.1)
extern template class aion::gameserver::taskmanager::AbstractFIFOPeriodicTaskManager<aion::gameserver::model::gameobjects::Creature>;

namespace aion::gameserver::taskmanager::tasks {

/**
 * Notifies the npcs that know a moving creature about the movement, every 500 ms.
 * <p>
 * C++: RefCounted through AbstractPeriodicTaskManager; the singleton is a never-released Ref (hub-headers.md §11.1). Java's public constructor is
 * protected (created only by getInstance).
 *
 * @author ATracer
 */
class MovementNotifyTask : public AbstractFIFOPeriodicTaskManager<model::gameobjects::Creature> {
	AION_MAKE_REF_FRIEND
protected:
	MovementNotifyTask();
	~MovementNotifyTask() override;

public:
	static MovementNotifyTask& getInstance();

protected:
	void callTask(model::gameobjects::Creature& creature) override;

public:
	/** Java package-private */
	void notifyCreatureMoved(model::gameobjects::Npc& npc, model::gameobjects::Creature& creature);

protected:
	std::string getCalledMethodName() override;
};

} // namespace aion::gameserver::taskmanager::tasks
