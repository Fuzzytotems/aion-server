#include "aion/gameserver/controllers/ControllerStandIns.h"

#include <typeinfo>

#include "aion/gameserver/GameServer.h"
#include "aion/gameserver/custom/pvpmap/PvpMapService.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/instance/handlers/InstanceHandler.h"
#include "aion/gameserver/utils/SimpleClassName.h"

namespace aion::gameserver::controllers::standins {

runtime::FutureRef followStartServiceNewFollowingToTargetCheckTask(model::gameobjects::Summon& follower, model::gameobjects::Creature& leading) {
	AION_UNPORTED();
}

void gameServerUpdateRatio(model::Race race, int32_t i) {
	GameServer::updateRatio(race, i);
}

bool pvpMapServiceIsOnPvPMap(model::gameobjects::Creature& creature) {
	return custom::pvpmap::PvpMapService::getInstance().isOnPvPMap(creature); // m5j CP8: PvpMapService is ported
}

bool isPvpMapHandler(instance::handlers::InstanceHandler& instanceHandler) {
	return utils::simpleClassName(typeid(instanceHandler)) == "PvpMapHandler";
}

} // namespace aion::gameserver::controllers::standins
