#include "aion/gameserver/handlers/ai/worlds/BlackenedGraveAI.h"

#include "aion/gameserver/ai/AIActions.h"

namespace aion::gameserver::handlers::ai::worlds {

AION_AI(BlackenedGraveAI, "blackened_grave");

// Java BlackenedGraveAI.java:20-23
bool BlackenedGraveAI::canThink() {
	return false;
}

// Java BlackenedGraveAI.java:25-30
void BlackenedGraveAI::handleDied() {
	GeneralNpcAI::handleDied();
	spawn(284262, getOwner().getX(), getOwner().getY(), getOwner().getZ(), int8_t{0});
	AIActions::deleteOwner(*this);
}

} // namespace aion::gameserver::handlers::ai::worlds
