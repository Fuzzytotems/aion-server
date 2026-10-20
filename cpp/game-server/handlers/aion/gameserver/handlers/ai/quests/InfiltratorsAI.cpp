#include "aion/gameserver/handlers/ai/quests/InfiltratorsAI.h"

#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"

namespace aion::gameserver::handlers::ai::quests {

AION_AI(InfiltratorsAI, "infiltrator");

// Java InfiltratorsAI.java:18-47. Correction of the Java code (owner's decision 2026-10-19, both branches; was J-A1A-1): no spawn when the
// switch gave no npc (the last of each chain and every other id); Java spawned npc 0, which fails with an error in the log
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
	if (spawnNpc != 0) // the correction (was J-A1A-1)
		spawn(spawnNpc, getOwner().getSpawn()->getX(), getOwner().getSpawn()->getY(), getOwner().getSpawn()->getZ(), getOwner().getSpawn()->getHeading());
}

} // namespace aion::gameserver::handlers::ai::quests
