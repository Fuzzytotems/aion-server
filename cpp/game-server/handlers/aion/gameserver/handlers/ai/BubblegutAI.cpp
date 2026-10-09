#include "aion/gameserver/handlers/ai/BubblegutAI.h"


namespace aion::gameserver::handlers::ai {

AION_AI(BubblegutAI, "bubblegut");

// Java BubblegutAI.java:18-22
void BubblegutAI::handleSpawned() {
	GeneralNpcAI::handleSpawned();
	AIActions::useSkill(*this, 16447);
}

} // namespace aion::gameserver::handlers::ai
