#include "aion/gameserver/handlers/ai/walkers/NaiaAI.h"

#include <optional>
#include <vector>

#include "aion/gameserver/ai/handler/MoveEventHandler.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/NpcShoutData.h"
#include "aion/gameserver/model/templates/npcshout/ShoutEventType.h"
#include "aion/gameserver/services/NpcShoutsService.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::walkers {

AION_AI(NaiaAI, "naia");

// Java NaiaAI.java:29-55 (a missing cannon or Qydro: Java's NullPointerException in PositionUtil.isInRange; Java's null shout list is
// std::nullopt, which shoutRandom ignores as Java does)
void NaiaAI::handleMoveArrived() {
	gameserver::ai::handler::MoveEventHandler::onMoveArrived(*this);
	runtime::Ptr<Npc> cannon = getPosition()->getWorldMapInstance()->getNpc(203145);
	runtime::Ptr<Npc> qydro = getPosition()->getWorldMapInstance()->getNpc(203125);
	bool isCannonNear = PositionUtil::isInRange(getOwner(), *cannon, static_cast<float>(getOwner().getAggroRange()));
	bool isQydroNear = PositionUtil::isInRange(getOwner(), *qydro, static_cast<float>(getOwner().getAggroRange()));
	std::optional<std::vector<const model::templates::npcshout::NpcShout*>> shouts;
	if (!saidCannon.get() && isCannonNear) {
		saidCannon = true;
		// TODO: she should get closer and turn to Cannon
		// getOwner().getPosition().setH((byte)60);
		shouts = dataholders::DataManager::NPC_SHOUT_DATA->getNpcShouts(getPosition()->getMapId(), getNpcId(),
			model::templates::npcshout::ShoutEventType::WALK_WAYPOINT, "2", 0);
		if (shouts.has_value())
			services::NpcShoutsService::getInstance().shoutRandom(getOwner(), nullptr, *shouts, 10);
	} else if (saidCannon.get() && !isCannonNear) {
		saidCannon = false;
	}
	if (!saidQydro.get() && isQydroNear) {
		saidQydro = true;
		shouts = dataholders::DataManager::NPC_SHOUT_DATA->getNpcShouts(getPosition()->getMapId(), getNpcId(),
			model::templates::npcshout::ShoutEventType::WALK_WAYPOINT, "1", 0);
		if (shouts.has_value())
			services::NpcShoutsService::getInstance().shoutRandom(getOwner(), nullptr, *shouts, 0);
	} else if (saidQydro.get() && !isQydroNear) {
		saidQydro = false;
	}
}

} // namespace aion::gameserver::handlers::ai::walkers
