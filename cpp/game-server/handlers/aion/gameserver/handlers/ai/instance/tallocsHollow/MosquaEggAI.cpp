#include "aion/gameserver/handlers/ai/instance/tallocsHollow/MosquaEggAI.h"

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::tallocsHollow {

AION_AI(MosquaEggAI, "mosquaegg");

// Java MosquaEggAI.java:24-31
void MosquaEggAI::handleSpawned() {
	AggressiveNpcAI::handleSpawned();
	supraklawSpawnTask.set(ThreadPoolManager::getInstance().schedule({this}, [this] {
		spawn(217132, getPosition()->getX(), getPosition()->getY(), getPosition()->getZ(), getPosition()->getHeading());
		getOwner().getController().delete_();
	}, 17000));
}

// Java MosquaEggAI.java:33-37
void MosquaEggAI::handleDespawned() {
	AggressiveNpcAI::handleDespawned();
	cancelSpawnTask();
}

// Java MosquaEggAI.java:39-43
void MosquaEggAI::handleDied() {
	AggressiveNpcAI::handleDied();
	cancelSpawnTask();
}

// Java MosquaEggAI.java:45-51
bool MosquaEggAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::ALLOW_DECAY:
		case AIQuestion::REWARD_AP_XP_DP_LOOT:
		case AIQuestion::REWARD_LOOT:
			return false;
		default:
			return AggressiveNpcAI::ask(question);
	}
}

// Java MosquaEggAI.java:53-58
void MosquaEggAI::cancelSpawnTask() {
	runtime::FutureRef task = supraklawSpawnTask.get();
	if (task != nullptr && !task->isDone()) {
		task->cancel(true);
		supraklawSpawnTask.set(nullptr);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::tallocsHollow
