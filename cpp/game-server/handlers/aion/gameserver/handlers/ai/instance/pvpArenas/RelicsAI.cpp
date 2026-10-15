#include "aion/gameserver/handlers/ai/instance/pvpArenas/RelicsAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/instance/handlers/InstanceHandler.h"
#include "aion/gameserver/model/instance/instancescore/InstanceScore.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::pvpArenas {

AION_AI(RelicsAI, "pvparenarelics");

// Java RelicsAI.java:23-30
void RelicsAI::handleDialogStart(Player& player) {
	runtime::Ptr<InstanceScore> instance = getPosition()->getWorldMapInstance()->getInstanceHandler()->getInstanceScore();
	if (instance != nullptr && !instance->isStartProgress()) {
		return;
	}
	ActionItemNpcAI::handleDialogStart(player);
}

// Java RelicsAI.java:32-43
void RelicsAI::handleUseItemFinish(Player& player) {
	if (!isRewarded.get()) {
		isRewarded = true;
		AIActions::handleUseItemFinish(*this, player);
		const int32_t npcId = getNpcId();
		if (npcId != 701187 && npcId != 701188) {
			AIActions::scheduleRespawn(*this);
		}
		AIActions::deleteOwner(*this);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::pvpArenas
