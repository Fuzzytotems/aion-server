#include "aion/gameserver/handlers/ai/NoInteractionAI.h"

#include "aion/gameserver/ai/handler/MoveEventHandler.h"
#include "aion/gameserver/ai/handler/ThinkEventHandler.h"
#include "aion/gameserver/model/gameobjects/Npc.h"

namespace aion::gameserver::handlers::ai {

AION_AI(NoInteractionAI, "no_interaction");

// Java NoInteractionAI.java:24-28
void NoInteractionAI::handleBeforeSpawned() {
	NpcAI::handleBeforeSpawned();
	getOwner().overrideNpcType(CreatureType::PEACE);
}

// Java NoInteractionAI.java:30-33
bool NoInteractionAI::canThink() {
	return false; // minimal thinking to just support walkers
}

// Java NoInteractionAI.java:35-38
void NoInteractionAI::think() {
	gameserver::ai::handler::ThinkEventHandler::onThink(*this);
}

// Java NoInteractionAI.java:40-44
void NoInteractionAI::handleMoveArrived() {
	NpcAI::handleMoveArrived();
	gameserver::ai::handler::MoveEventHandler::onMoveArrived(*this);
}

// Java NoInteractionAI.java:46-49
float NoInteractionAI::modifyDamage(Creature& /*attacker*/, float /*damage*/, runtime::Ptr<Effect> /*effect*/) {
	return 0;
}

} // namespace aion::gameserver::handlers::ai
