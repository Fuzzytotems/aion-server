#include "aion/gameserver/handlers/ai/worlds/panesterra/ahserionsflight/EreshkigalsVoiceAI.h"

#include "aion/gameserver/controllers/NpcController.h"

namespace aion::gameserver::handlers::ai::worlds::panesterra::ahserionsflight {

AION_AI(EreshkigalsVoiceAI, "ereshkigals_voice");

// Java EreshkigalsVoiceAI.java:24-32
void EreshkigalsVoiceAI::handleSpawned() {
	NpcAI::handleSpawned();
	idleTask.set(ThreadPoolManager::getInstance().schedule({this}, [this] {
		PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(getOwner()), 1501160); // I won't forgive you, Fregion!!!
		getOwner().getController().addTask(model::TaskId::DESPAWN, ThreadPoolManager::getInstance().schedule({this}, [this] {
			getOwner().getController().deleteIfAliveOrCancelRespawn();
		}, 2000));
	}, 30000));
}

// Java EreshkigalsVoiceAI.java:34-39
void EreshkigalsVoiceAI::handleDespawned() {
	runtime::FutureRef task = idleTask.get();
	if (task != nullptr && !task->isDone())
		task->cancel(true);
	NpcAI::handleDespawned();
}

} // namespace aion::gameserver::handlers::ai::worlds::panesterra::ahserionsflight
