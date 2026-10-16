#include "aion/gameserver/ai/handler/ActivateEventHandler.h"

#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/NpcAI.h"
#include "aion/gameserver/ai/poll/AIQuestion.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/model/gameobjects/Npc.h"

namespace aion::gameserver::ai::handler {

void ActivateEventHandler::onActivate(NpcAI& npcAI) {
	if (npcAI.isInState(AIState::IDLE)) {
		npcAI.getOwner().updateKnownlist();
		npcAI.think();
	}
}

void ActivateEventHandler::onDeactivate(NpcAI& npcAI) {
	npcAI.think();
	model::gameobjects::Npc& npc = npcAI.getOwner();
	npc.updateKnownlist();
	npc.getController().loseAggro(false);
	if (npcAI.ask(poll::AIQuestion::REMOVE_EFFECTS_ON_MAP_REGION_DEACTIVATE))
		npc.getEffectController()->removeAllEffects();
}

} // namespace aion::gameserver::ai::handler
