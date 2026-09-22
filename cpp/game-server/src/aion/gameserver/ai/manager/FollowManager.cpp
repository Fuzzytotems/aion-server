#include "aion/gameserver/ai/manager/FollowManager.h"

#include "aion/gameserver/ai/AILogger.h"
#include "aion/gameserver/ai/NpcAI.h"
#include "aion/gameserver/controllers/movement/NpcMoveController.h"
#include "aion/gameserver/model/gameobjects/Npc.h"

namespace aion::gameserver::ai::manager {

void FollowManager::targetTooFar(NpcAI& npcAI) {
	model::gameobjects::Npc& npc = npcAI.getOwner();
	if (npcAI.isLogging()) {
		AILogger::info(npcAI, "Follow manager - targetTooFar");
	}
	if (npcAI.isMoveSupported()) {
		npc.getMoveController()->moveToTargetObject();
	}
}

} // namespace aion::gameserver::ai::manager
