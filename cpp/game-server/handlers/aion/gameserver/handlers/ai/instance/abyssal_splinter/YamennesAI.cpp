#include "aion/gameserver/handlers/ai/instance/abyssal_splinter/YamennesAI.h"

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::abyssal_splinter {

AION_AI(YamennesAI, "yamennes");

// Java YamennesAI.java:33-40
void YamennesAI::handleAttack(runtime::Ptr<Creature> creature) {
	AggressiveNpcAI::handleAttack(creature);
	if (isStart.compareAndSet(false, true)) {
		PacketSendUtility::broadcastMessage(getOwner(), 1500013); // Those who threaten the artefact shall be returned to the flow of Aether!
		startTasks();
	}
}

// Java YamennesAI.java:42-45
void YamennesAI::startTasks() {
	enrageTask.set(ThreadPoolManager::getInstance().schedule({this}, [this] { getOwner().queueSkill(19098, 55); }, 600000));
	portalTask.set(ThreadPoolManager::getInstance().schedule({this}, [this] { spawnPortals(false); }, 60000));
}

// Java YamennesAI.java:47-56
void YamennesAI::onHealingDebuff() {
	runtime::Ptr<WorldMapInstance> instance = getPosition()->getWorldMapInstance();
	deleteNpcs(instance->getNpcs({282107}));
	getOwner().queueSkill(19282, 55);
	spawn(282107, getOwner().getX() + 10, getOwner().getY() - 10, getOwner().getZ(), int8_t{0});
	spawn(282107, getOwner().getX() - 10, getOwner().getY() + 10, getOwner().getZ(), int8_t{0});
	spawn(282107, getOwner().getX() + 10, getOwner().getY() + 10, getOwner().getZ(), int8_t{0});
	getOwner().clearAttackedCount();
	PacketSendUtility::broadcastToMap(getOwner(), SM_SYSTEM_MESSAGE::STR_MSG_IDAbRe_Core_NmdD_ResetAggro());
}

// Java YamennesAI.java:58-76
void YamennesAI::spawnPortals(bool isTopSpawn) {
	runtime::Ptr<Npc> portalA = getPosition()->getWorldMapInstance()->getNpc(282014);
	runtime::Ptr<Npc> portalB = getPosition()->getWorldMapInstance()->getNpc(282015);
	runtime::Ptr<Npc> portalC = getPosition()->getWorldMapInstance()->getNpc(282131);
	if (portalA == nullptr && portalB == nullptr && portalC == nullptr) {
		PacketSendUtility::broadcastToMap(getOwner(), SM_SYSTEM_MESSAGE::STR_MSG_IDAbRe_Core_NmdD_SummonStart());
		if (isTopSpawn) {
			spawn(282014, 288.10f, 741.95f, 216.81f, int8_t{3});
			spawn(282015, 375.05f, 750.67f, 216.82f, int8_t{59});
			spawn(282131, 341.33f, 699.38f, 216.86f, int8_t{59});
		} else {
			spawn(282014, 303.69f, 736.35f, 198.7f, int8_t{0});
			spawn(282015, 335.19f, 708.92f, 198.9f, int8_t{35});
			spawn(282131, 360.23f, 741.07f, 198.7f, int8_t{0});
		}
	}
	ThreadPoolManager::getInstance().schedule({this}, [this] { onHealingDebuff(); }, 3000);
	portalTask.set(ThreadPoolManager::getInstance().schedule({this}, [this, isTopSpawn] { spawnPortals(!isTopSpawn); }, 60000));
}

// Java YamennesAI.java:78-80
void YamennesAI::deleteNpcs(const std::vector<runtime::Ptr<Npc>>& npcs) {
	for (const runtime::Ptr<Npc>& n : npcs) {
		if (n != nullptr)
			n->getController().delete_();
	}
}

// Java YamennesAI.java:82-87
void YamennesAI::cancelTasks() {
	runtime::FutureRef portal = portalTask.get();
	if (portal != nullptr && !portal->isDone())
		portal->cancel(true);
	runtime::FutureRef enrage = enrageTask.get();
	if (enrage != nullptr && !enrage->isDone())
		enrage->cancel(true);
}

// Java YamennesAI.java:89-94
void YamennesAI::handleBackHome() {
	cancelTasks();
	AggressiveNpcAI::handleBackHome();
	getOwner().getController().delete_();
}

// Java YamennesAI.java:96-101
void YamennesAI::handleDespawned() {
	cancelTasks();
	deleteNpcs(getPosition()->getWorldMapInstance()->getNpcs({282107}));
	AggressiveNpcAI::handleDespawned();
}

// Java YamennesAI.java:103-108
void YamennesAI::handleDied() {
	cancelTasks();
	deleteNpcs(getPosition()->getWorldMapInstance()->getNpcs({282107}));
	AggressiveNpcAI::handleDied();
}

// Java YamennesAI.java:110-116
bool YamennesAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::REWARD_LOOT:
		case AIQuestion::REWARD_AP:
			return false;
		default:
			return AggressiveNpcAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::abyssal_splinter
