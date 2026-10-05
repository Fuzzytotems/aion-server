#include "aion/gameserver/taskmanager/tasks/TeamMoveUpdater.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/alliance/PlayerAllianceService.h"
#include "aion/gameserver/model/team/common/legacy/GroupEvent.h"
#include "aion/gameserver/model/team/common/legacy/PlayerAllianceEvent.h"
#include "aion/gameserver/model/team/group/PlayerGroupService.h"

// the one explicit instantiation for the two team updaters (TeamMoveUpdater.h, TeamStatUpdater.h)
template class aion::gameserver::taskmanager::AbstractFIFOPeriodicTaskManager<aion::gameserver::model::gameobjects::player::Player>;

namespace aion::gameserver::taskmanager::tasks {

TeamMoveUpdater& TeamMoveUpdater::getInstance() {
	// Java: SingletonHolder; a never-released Ref (hub-headers.md §11.1)
	static const runtime::Ref<TeamMoveUpdater>& instance = *new runtime::Ref<TeamMoveUpdater>(runtime::makeRef<TeamMoveUpdater>());
	return *instance;
}

TeamMoveUpdater::TeamMoveUpdater() : AbstractFIFOPeriodicTaskManager(2000, "TeamMoveUpdater") {
}

TeamMoveUpdater::~TeamMoveUpdater() = default;

void TeamMoveUpdater::callTask(model::gameobjects::player::Player& player) {
	if (player.isOnline()) {
		if (player.isInGroup()) {
			model::team::group::PlayerGroupService::updateGroup(player, model::team::common::legacy::GroupEvent::MOVEMENT);
		} else if (player.isInAlliance()) {
			model::team::alliance::PlayerAllianceService::updateAlliance(player, model::team::common::legacy::PlayerAllianceEvent::MOVEMENT);
		}
	}
}

std::string TeamMoveUpdater::getCalledMethodName() {
	return "teamMoveUpdate()";
}

} // namespace aion::gameserver::taskmanager::tasks
