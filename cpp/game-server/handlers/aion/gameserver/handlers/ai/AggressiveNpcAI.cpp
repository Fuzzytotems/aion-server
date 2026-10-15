#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

#include "aion/gameserver/ai/handler/AggroEventHandler.h"
#include "aion/gameserver/ai/handler/CreatureEventHandler.h"

namespace aion::gameserver::handlers::ai {

AION_AI(AggressiveNpcAI, "aggressive");

void AggressiveNpcAI::handleCreatureSee(Creature& creature) {
	CreatureEventHandler::onCreatureSee(*this, creature);
}

void AggressiveNpcAI::handleCreatureAggro(Creature& creature) {
	if (canThink())
		AggroEventHandler::onAggro(*this, creature);
}

bool AggressiveNpcAI::handleCreatureNeedsSupportByGuard(Creature& creature) {
	return AggroEventHandler::onCreatureNeedsSupportByGuard(*this, creature);
}

} // namespace aion::gameserver::handlers::ai
