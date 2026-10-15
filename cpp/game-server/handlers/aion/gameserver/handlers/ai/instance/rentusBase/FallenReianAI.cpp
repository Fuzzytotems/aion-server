#include "aion/gameserver/handlers/ai/instance/rentusBase/FallenReianAI.h"

#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::rentusBase {

AION_AI(FallenReianAI, "fallen_reian");

// Java FallenReianAI.java:24-33
void FallenReianAI::handleCreatureMoved(Creature& creature) {
	if (isCollapsed.get())
		return;
	runtime::Ptr<Player> player = runtime::as<Player>(runtime::Ptr<Creature>(creature));
	if (player == nullptr)
		return;
	if (PositionUtil::isInRange(getOwner(), *player, 20) && isCollapsed.compareAndSet(false, true))
		getPosition()->getWorldMapInstance()->setDoorState(getNpcId() == 799661 ? 16 : 54, true);
}

} // namespace aion::gameserver::handlers::ai::instance::rentusBase
