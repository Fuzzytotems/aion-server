#include "aion/gameserver/handlers/ai/quests/InfiltratorsAI.h"

#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"

namespace aion::gameserver::handlers::ai::quests {

AION_AI(InfiltratorsAI, "infiltrator");

// Java InfiltratorsAI.java:18-47
void InfiltratorsAI::handleDied() {
	AggressiveNpcAI::handleDied();
	int32_t ownerNpcId = getOwner().getNpcId(); // Java: owner (it hides the AI's member)
	int32_t spawnNpc = 0;
	switch (ownerNpcId) {
		case 282913:
			spawnNpc = 282914;
			break;
		case 282918:
			spawnNpc = 282920;
			break;
		case 282920:
			spawnNpc = 282922;
			break;
		case 282917:
			spawnNpc = 282915;
			break;
		case 282915:
			spawnNpc = 282916;
			break;
		case 282919:
			spawnNpc = 282921;
			break;
		case 282921:
			spawnNpc = 282923;
			break;
	}
	spawn(spawnNpc, getOwner().getSpawn()->getX(), getOwner().getSpawn()->getY(), getOwner().getSpawn()->getZ(), getOwner().getSpawn()->getHeading());
}

} // namespace aion::gameserver::handlers::ai::quests
