#include "aion/gameserver/handlers/ai/instance/unstableSplinterpath/UnstableYamennesAI.h"

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::unstableSplinterpath {

AION_AI(UnstableYamennesAI, "unstableyamennes");

// Java UnstableYamennesAI.java:33-40
void UnstableYamennesAI::handleAttack(runtime::Ptr<Creature> creature) {
	AggressiveNpcAI::handleAttack(creature);
	if (isStart.compareAndSet(false, true)) {
		PacketSendUtility::broadcastMessage(getOwner(), 1500013); // Those who threaten the artefact shall be returned to the flow of Aether!
		startTasks();
	}
}

// Java UnstableYamennesAI.java:42-45
void UnstableYamennesAI::startTasks() {
	enrageTask.set(ThreadPoolManager::getInstance().schedule({this}, [this] { getOwner().queueSkill(19098, 55, 0); }, 600000));
	portalTask.set(ThreadPoolManager::getInstance().schedule({this}, [this] { spawnPortals(false); }, 60000));
}

// Java UnstableYamennesAI.java:47-56
void UnstableYamennesAI::onHealingDebuff() {
	runtime::Ptr<WorldMapInstance> instance = getPosition()->getWorldMapInstance();
	deleteNpcs(instance->getNpcs({219586}));
	getOwner().queueSkill(19282, 55);
	spawn(219586, getOwner().getX() + 10, getOwner().getY() - 10, getOwner().getZ(), int8_t{0});
	spawn(219586, getOwner().getX() - 10, getOwner().getY() + 10, getOwner().getZ(), int8_t{0});
	spawn(219586, getOwner().getX() + 10, getOwner().getY() + 10, getOwner().getZ(), int8_t{0});
	getOwner().clearAttackedCount();
	PacketSendUtility::broadcastToMap(getOwner(), SM_SYSTEM_MESSAGE::STR_MSG_IDAbRe_Core_NmdD_ResetAggro());
}

// Java UnstableYamennesAI.java:58-76
void UnstableYamennesAI::spawnPortals(bool isTopSpawn) {
	runtime::Ptr<Npc> portalA = getPosition()->getWorldMapInstance()->getNpc(219567);
	runtime::Ptr<Npc> portalB = getPosition()->getWorldMapInstance()->getNpc(219579);
	runtime::Ptr<Npc> portalC = getPosition()->getWorldMapInstance()->getNpc(219580);
	if (portalA == nullptr && portalB == nullptr && portalC == nullptr) {
		PacketSendUtility::broadcastToMap(getOwner(), SM_SYSTEM_MESSAGE::STR_MSG_IDAbRe_Core_NmdD_SummonStart());
		if (isTopSpawn) {
			spawn(219567, 288.10f, 741.95f, 216.81f, int8_t{3});
			spawn(219579, 375.05f, 750.67f, 216.82f, int8_t{59});
			spawn(219580, 341.33f, 699.38f, 216.86f, int8_t{59});
		} else {
			spawn(219567, 303.69f, 736.35f, 198.7f, int8_t{0});
			spawn(219579, 335.19f, 708.92f, 198.9f, int8_t{35});
			spawn(219580, 360.23f, 741.07f, 198.7f, int8_t{0});
		}
	}
	ThreadPoolManager::getInstance().schedule({this}, [this] { onHealingDebuff(); }, 3000);
	portalTask.set(ThreadPoolManager::getInstance().schedule({this}, [this, isTopSpawn] { spawnPortals(!isTopSpawn); }, 60000));
}

// Java UnstableYamennesAI.java:78-80
void UnstableYamennesAI::deleteNpcs(const std::vector<runtime::Ptr<Npc>>& npcs) {
	for (const runtime::Ptr<Npc>& n : npcs) {
		if (n != nullptr)
			n->getController().delete_();
	}
}

// Java UnstableYamennesAI.java:82-87
void UnstableYamennesAI::cancelTasks() {
	runtime::FutureRef portal = portalTask.get();
	if (portal != nullptr && !portal->isDone())
		portal->cancel(true);
	runtime::FutureRef enrage = enrageTask.get();
	if (enrage != nullptr && !enrage->isDone())
		enrage->cancel(true);
}

// Java UnstableYamennesAI.java:89-94
void UnstableYamennesAI::handleBackHome() {
	cancelTasks();
	AggressiveNpcAI::handleBackHome();
	getOwner().getController().delete_();
}

// Java UnstableYamennesAI.java:96-101
void UnstableYamennesAI::handleDespawned() {
	cancelTasks();
	deleteNpcs(getPosition()->getWorldMapInstance()->getNpcs({219586}));
	AggressiveNpcAI::handleDespawned();
}

// Java UnstableYamennesAI.java:103-108
void UnstableYamennesAI::handleDied() {
	cancelTasks();
	deleteNpcs(getPosition()->getWorldMapInstance()->getNpcs({219586}));
	AggressiveNpcAI::handleDied();
}

// Java UnstableYamennesAI.java:110-116
bool UnstableYamennesAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::REWARD_LOOT:
		case AIQuestion::REWARD_AP:
			return false;
		default:
			return AggressiveNpcAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::unstableSplinterpath
