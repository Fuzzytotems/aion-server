#include "aion/gameserver/handlers/ai/instance/nightmareCircus/CircusBoxesAI.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::nightmareCircus {

AION_AI(CircusBoxesAI, "circusboxes");

// Java CircusBoxesAI.java:24-28
void CircusBoxesAI::handleSpawned() {
	NpcAI::handleSpawned();
	spawnJester();
}

// Java CircusBoxesAI.java:30-35
void CircusBoxesAI::handleDespawned() {
	NpcAI::handleDespawned();
	cancelspawnJesterTask();
	despawnNpcs({233462, 233463});
}

// Java CircusBoxesAI.java:37-42
void CircusBoxesAI::handleDied() {
	NpcAI::handleDied();
	PacketSendUtility::broadcastMessage(getOwner(), 1501144);
	cancelspawnJesterTask();
}

// Java CircusBoxesAI.java:44-48
void CircusBoxesAI::cancelspawnJesterTask() {
	runtime::FutureRef task = spawnJesterTask.get();
	if (task != nullptr && !task->isDone()) {
		task->cancel(true);
	}
}

// Java CircusBoxesAI.java:50-69
void CircusBoxesAI::spawnJester() {
	spawnJesterTask.set(ThreadPoolManager::getInstance().schedule({this}, [this] {
		if (!isDead()) {
			getOwner().getController().delete_();
			int32_t count = commons::utils::Rnd::get(3, 5);
			for (int32_t i = 0; i < count; i++) {
				switch (getOwner().getNpcId()) {
					case 831348:
						rndSpawnInRange(233462, 3, 8);
						break;
					case 831349:
						rndSpawnInRange(233463, 3, 8);
						break;
					default:
						return;
				}
			}
		}
	}, 33000));
}

// Java CircusBoxesAI.java:71-75
void CircusBoxesAI::despawnNpcs(std::initializer_list<int32_t> npcIds) {
	for (const runtime::Ptr<Npc>& npc : getOwner().getPosition()->getWorldMapInstance()->getNpcs(npcIds)) {
		npc->getController().delete_();
	}
}

} // namespace aion::gameserver::handlers::ai::instance::nightmareCircus
