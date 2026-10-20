#include "aion/gameserver/handlers/ai/worlds/NamelessGraveAI.h"

namespace aion::gameserver::handlers::ai::worlds {

AION_AI(NamelessGraveAI, "namelessgrave");

// Java NamelessGraveAI.java:21-24
bool NamelessGraveAI::canThink() {
	return false;
}

// Java NamelessGraveAI.java:26-33
void NamelessGraveAI::handleDied() {
	GeneralNpcAI::handleDied();
	if (isSpawned.compareAndSet(false, true)) {
		rndSpawnInRange(283905, 1, 2);
		rndSpawnInRange(283905, 1, 2);
	}
}

} // namespace aion::gameserver::handlers::ai::worlds
