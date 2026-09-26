#include "aion/gameserver/ai/handler/MoveEventHandler.h"

#include "aion/gameserver/ai/NpcAI.h"
#include "aion/gameserver/ai/handler/TargetEventHandler.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/model/gameobjects/Npc.h"

namespace aion::gameserver::ai::handler {

void MoveEventHandler::onMoveValidate(NpcAI& npcAI) {
	if (npcAI.getOwner().canPerformMove()) {
		npcAI.getOwner().getController().onMove();
		TargetEventHandler::onTargetTooFar(npcAI);
	}
}

void MoveEventHandler::onMoveArrived(NpcAI& npcAI) {
	npcAI.getOwner().getController().onMove();
	TargetEventHandler::onTargetReached(npcAI);
}

} // namespace aion::gameserver::ai::handler
