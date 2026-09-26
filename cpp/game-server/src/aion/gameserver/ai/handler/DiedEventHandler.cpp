#include "aion/gameserver/ai/handler/DiedEventHandler.h"

#include "aion/gameserver/ai/AILogger.h"
#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/AISubState.h"
#include "aion/gameserver/ai/NpcAI.h"
#include "aion/gameserver/ai/handler/ShoutEventHandler.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"

namespace aion::gameserver::ai::handler {

void DiedEventHandler::onDie(NpcAI& npcAI) {
	if (npcAI.isLogging()) {
		AILogger::info(npcAI, "onDie");
	}

	ShoutEventHandler::onDied(npcAI);

	npcAI.setStateIfNot(AIState::DIED);
	npcAI.setSubStateIfNot(AISubState::NONE);
	npcAI.getOwner().setTarget(nullptr);
}

} // namespace aion::gameserver::ai::handler
