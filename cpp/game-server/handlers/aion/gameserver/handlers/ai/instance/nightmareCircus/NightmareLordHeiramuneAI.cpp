#include "aion/gameserver/handlers/ai/instance/nightmareCircus/NightmareLordHeiramuneAI.h"

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::nightmareCircus {

AION_AI(NightmareLordHeiramuneAI, "nightmarelordheiramune");

// Java NightmareLordHeiramuneAI.java:27-31
void NightmareLordHeiramuneAI::handleAttack(runtime::Ptr<Creature> creature) {
	AggressiveNpcAI::handleAttack(creature);
	hpPhases->tryEnterNextPhase(*this);
}

// Java NightmareLordHeiramuneAI.java:33-42
void NightmareLordHeiramuneAI::handleHpPhase(int32_t phaseHpPercent) {
	switch (phaseHpPercent) {
		case 80:
			startSpawnTask();
			break;
		case 50:
			PacketSendUtility::broadcastMessage(getOwner(), 1501138);
			spawn(233162, getOwner().getX() + 5, getOwner().getY() + 5, getOwner().getZ(), getOwner().getHeading());
			break;
		default:
			break;
	}
}

// Java NightmareLordHeiramuneAI.java:44-50
void NightmareLordHeiramuneAI::startSpawnTask() {
	spawnTask.set(ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] {
		if (!isDead()) {
			spawnHelpers();
		}
	}, 0, 20000));
}

// Java NightmareLordHeiramuneAI.java:52-56
void NightmareLordHeiramuneAI::cancelTask() {
	runtime::FutureRef task = spawnTask.get();
	if (task != nullptr && !task->isCancelled()) {
		task->cancel(true);
	}
}

// Java NightmareLordHeiramuneAI.java:58-63
void NightmareLordHeiramuneAI::handleDied() {
	AggressiveNpcAI::handleDied();
	cancelTask();
	despawnNpcs({233457, 233162});
}

// Java NightmareLordHeiramuneAI.java:65-69
void NightmareLordHeiramuneAI::handleDespawned() {
	AggressiveNpcAI::handleDespawned();
	cancelTask();
}

// Java NightmareLordHeiramuneAI.java:71-76
void NightmareLordHeiramuneAI::handleBackHome() {
	AggressiveNpcAI::handleBackHome();
	cancelTask();
	hpPhases->reset();
}

// Java NightmareLordHeiramuneAI.java:78-82
void NightmareLordHeiramuneAI::despawnNpcs(std::initializer_list<int32_t> npcIds) {
	for (const runtime::Ptr<Npc>& npc : getOwner().getPosition()->getWorldMapInstance()->getNpcs(npcIds)) {
		npc->getController().delete_();
	}
}

// Java NightmareLordHeiramuneAI.java:84-88
void NightmareLordHeiramuneAI::spawnHelpers() {
	PacketSendUtility::broadcastMessage(getOwner(), 1501139);
	spawn(233457, 521.585f, 510.16528f, 199.59279f, int8_t{30});
	spawn(233457, 523.3747f, 621.1362f, 208.05113f, int8_t{90});
}

} // namespace aion::gameserver::handlers::ai::instance::nightmareCircus
