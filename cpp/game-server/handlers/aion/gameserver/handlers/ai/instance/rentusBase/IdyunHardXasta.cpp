#include "aion/gameserver/handlers/ai/instance/rentusBase/IdyunHardXasta.h"

#include "aion/gameserver/controllers/movement/NpcMoveController.h"
#include "aion/gameserver/instance/handlers/InstanceHandler.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::rentusBase {

AION_AI(IdyunHardXasta, "idyun_hard_xasta");

// Java IdyunHardXasta.java:22-25
bool IdyunHardXasta::canThink() {
	return false;
}

// Java IdyunHardXasta.java:27-34
void IdyunHardXasta::handleMoveArrived() {
	GeneralNpcAI::handleMoveArrived();
	if (getOwner().getMoveController()->isStop()) {
		if (isDestinationReached.compareAndSet(false, true))
			getOwner().getPosition()->getWorldMapInstance()->getInstanceHandler()->onSpecialEvent(getOwner());
	}
}

} // namespace aion::gameserver::handlers::ai::instance::rentusBase
