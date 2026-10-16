#include "aion/gameserver/handlers/ai/instance/crucibleChallenge/BarrelAI.h"

#include "aion/gameserver/ai/AIActions.h"

namespace aion::gameserver::handlers::ai::instance::crucibleChallenge {

AION_AI(BarrelAI, "barrel");

// Java BarrelAI.java:18-26
void BarrelAI::handleDied() {
	NpcAI::handleDied();
	switch (getNpcId()) {
		case 218560:
			rndSpawnInRange(218561, 4);
			break;
		case 217840:
			rndSpawnInRange(217841, 4);
			break;
		default:
			break;
	}
	AIActions::deleteOwner(*this);
}

} // namespace aion::gameserver::handlers::ai::instance::crucibleChallenge
