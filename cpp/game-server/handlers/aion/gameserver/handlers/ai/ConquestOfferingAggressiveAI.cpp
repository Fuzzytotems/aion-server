#include "aion/gameserver/handlers/ai/ConquestOfferingAggressiveAI.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai {

AION_AI(ConquestOfferingAggressiveAI, "conquest_offering_aggressive");

// Java ConquestOfferingAggressiveAI.java:20-24
void ConquestOfferingAggressiveAI::handleSpawned() {
	AggressiveNpcAI::handleSpawned();
	findAndSetCreator();
}

// Java ConquestOfferingAggressiveAI.java:26-29
void ConquestOfferingAggressiveAI::findAndSetCreator() {
	if (getCreatorId() != 0) {
		runtime::Ptr<Npc> npc = runtime::as<Npc>(getPosition()->getWorldMapInstance()->getObject(getCreatorId())); // Java: the pattern variable
		if (npc != nullptr)
			spawner.set(runtime::Ref<Npc>(npc));
	}
}

// Java ConquestOfferingAggressiveAI.java:31-38
void ConquestOfferingAggressiveAI::handleDied() {
	runtime::Ref<Npc> spawnerNpc = spawner.get();
	if (spawnerNpc != nullptr && !spawnerNpc->isDead()) {
		spawnerNpc->getAi().onCustomEvent(1); // notify spawner that npc died
		spawnRandomNpc();
	}
	AggressiveNpcAI::handleDied();
}

// Java ConquestOfferingAggressiveAI.java:40-53: spawn a shugo or a portal
void ConquestOfferingAggressiveAI::spawnRandomNpc() {
	int32_t npcId = 0;
	if (commons::utils::Rnd::chance() < 55) {
		if (commons::utils::Rnd::chance() < 45) { // spawn a shugo
			npcId = 856175 + commons::utils::Rnd::get(0, 3);
		} else { // spawn a portal
			npcId = getOwner().getWorldId() == 210050000 ? 833018 : 833021;
		}
	}
	if (npcId != 0)
		spawn(npcId, getOwner().getX() + 0.3f, getOwner().getY() + 0.3f, getOwner().getZ() + 0.2f, getOwner().getHeading());
}

} // namespace aion::gameserver::handlers::ai
