#include "aion/gameserver/controllers/ControllerStandIns.h"

#include <typeinfo>

#include "aion/gameserver/GameServer.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/instance/handlers/InstanceHandler.h"
#include "aion/gameserver/utils/SimpleClassName.h"

namespace aion::gameserver::controllers::standins {

void playerTeamDistributionServiceDoReward(model::team::TemporaryPlayerTeam& team, float damagePercent, model::gameobjects::Npc& owner,
	model::gameobjects::AionObject& winner, attack::TeamDamageList& damageList) {
	AION_UNPORTED();
}

runtime::FutureRef followStartServiceNewFollowingToTargetCheckTask(model::gameobjects::Summon& follower, model::gameobjects::Creature& leading) {
	AION_UNPORTED();
}

void playerGroupServiceRemovePlayer(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void playerAllianceServiceRemovePlayer(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

RiftEnumData riftEnumData(services::rift::RiftEnum riftTemplate) {
	AION_UNPORTED();
}

void teamMoveUpdaterAdd(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void teamStatUpdaterAdd(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void gameServerUpdateRatio(model::Race race, int32_t i) {
	GameServer::updateRatio(race, i);
}

bool pvpMapServiceIsOnPvPMap(model::gameobjects::Creature& creature) {
	AION_UNPORTED();
}

bool isPvpMapHandler(instance::handlers::InstanceHandler& instanceHandler) {
	return utils::simpleClassName(typeid(instanceHandler)) == "PvpMapHandler";
}

} // namespace aion::gameserver::controllers::standins
