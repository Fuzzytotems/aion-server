#include "aion/gameserver/taskmanager/tasks/TeamStatUpdater.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/alliance/PlayerAllianceService.h"
#include "aion/gameserver/model/team/common/legacy/GroupEvent.h"
#include "aion/gameserver/model/team/common/legacy/PlayerAllianceEvent.h"
#include "aion/gameserver/model/team/group/PlayerGroupService.h"

namespace aion::gameserver::taskmanager::tasks {

TeamStatUpdater& TeamStatUpdater::getInstance() {
	// Java: SingletonHolder; a never-released Ref (hub-headers.md §11.1)
	static const runtime::Ref<TeamStatUpdater>& instance = *new runtime::Ref<TeamStatUpdater>(runtime::makeRef<TeamStatUpdater>());
	return *instance;
}

TeamStatUpdater::TeamStatUpdater() : AbstractFIFOPeriodicTaskManager(500, "TeamStatUpdater") {
}

TeamStatUpdater::~TeamStatUpdater() = default;

void TeamStatUpdater::callTask(model::gameobjects::player::Player& player) {
	if (player.isOnline()) {
		if (player.isInGroup()) {
			model::team::group::PlayerGroupService::updateGroup(player, model::team::common::legacy::GroupEvent::MOVEMENT);
		} else if (player.isInAlliance()) {
			model::team::alliance::PlayerAllianceService::updateAlliance(player, model::team::common::legacy::PlayerAllianceEvent::MOVEMENT);
		}
	}
}

std::string TeamStatUpdater::getCalledMethodName() {
	return "teamStatUpdate()";
}

} // namespace aion::gameserver::taskmanager::tasks
