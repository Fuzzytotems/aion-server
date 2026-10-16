#include "aion/gameserver/handlers/ai/instance/abyss/IllusionGateAI.h"

#include "aion/commons/utils/Rnd.h"

namespace aion::gameserver::handlers::ai::instance::abyss {

AION_AI(IllusionGateAI, "illusion_gate");

// Java IllusionGateAI.java:27-35
void IllusionGateAI::handleSpawned() {
	NpcAI::handleSpawned();
	spawnTask.set(ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] {
		for (int32_t i = 0; i < 2; i++) {
			rndSpawnInRange(getNpcId() + commons::utils::Rnd::get(1, 3), 0, 1);
		}
	}, 10000, 30000));
}

// Java IllusionGateAI.java:37-41
void IllusionGateAI::handleDied() {
	NpcAI::handleDied();
	cancelTask();
}

// Java IllusionGateAI.java:43-46
void IllusionGateAI::handleDespawned() {
	NpcAI::handleDespawned();
	cancelTask();
}

// Java IllusionGateAI.java:48-51
void IllusionGateAI::cancelTask() {
	runtime::FutureRef task = spawnTask.get();
	if (task != nullptr && !task->isCancelled())
		task->cancel(true);
}

} // namespace aion::gameserver::handlers::ai::instance::abyss
