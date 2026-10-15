#pragma once

#include <string>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/taskmanager/AbstractFIFOPeriodicTaskManager.h"
#include "aion/gameserver/taskmanager/tasks/fwd.h"

// instantiated in TeamMoveUpdater.cpp, so this header needs no complete Player (hub-headers.md §3.1)
extern template class aion::gameserver::taskmanager::AbstractFIFOPeriodicTaskManager<aion::gameserver::model::gameobjects::player::Player>;

namespace aion::gameserver::taskmanager::tasks {

/**
 * Supports PlayerGroup and PlayerAlliance movement updating.
 * <p>
 * C++: RefCounted through AbstractPeriodicTaskManager; the singleton is a never-released Ref (hub-headers.md §11.1, the MovementNotifyTask
 * precedent). Java's public constructor is protected (created only by getInstance). m5g-plan.md GR-04, header request m5g-9.
 *
 * @author Sarynth
 */
class TeamMoveUpdater final : public AbstractFIFOPeriodicTaskManager<model::gameobjects::player::Player> {
	AION_MAKE_REF_FRIEND
protected:
	TeamMoveUpdater();
	~TeamMoveUpdater() override;

public:
	static TeamMoveUpdater& getInstance();

protected:
	void callTask(model::gameobjects::player::Player& player) override;

	std::string getCalledMethodName() override;
};

} // namespace aion::gameserver::taskmanager::tasks
