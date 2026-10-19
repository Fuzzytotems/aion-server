#include "aion/gameserver/handlers/ai/instance/illuminaryObelisk/DynatoumAI.h"

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::illuminaryObelisk {

AION_AI(DynatoumAI, "dynatoum");

// Java DynatoumAI.java:30-37
void DynatoumAI::handleAttack(runtime::Ptr<Creature> creature) {
	SummonerAI::handleAttack(creature);
	if (isStarted.compareAndSet(false, true))
		scheduleDespawn(1);
	if (getLifeStats()->getHpPercentage() <= 70 && areEntriesRemoved.compareAndSet(false, true))
		removeBossEntries();
}

// Java DynatoumAI.java:39-42
void DynatoumAI::removeBossEntries() {
	PacketSendUtility::broadcastToMap(getOwner(), SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_BOSS_PORTAL_DESTROY());
	for (const runtime::Ptr<Npc>& p : getPosition()->getWorldMapInstance()->getNpcs({702216})) {
		if (p != nullptr)
			p->getController().delete_();
	}
}

// Java DynatoumAI.java:44-66
void DynatoumAI::scheduleDespawn(int32_t delayInSec) {
	despawnTask = ThreadPoolManager::getInstance().schedule({this}, [this, delayInSec] {
		if (!isDead()) {
			switch (delayInSec) {
				case 1:
					PacketSendUtility::broadcastToMap(getOwner(), SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_BOSS_TIMER_01());
					scheduleDespawn(300);
					break;
				case 300:
					PacketSendUtility::broadcastToMap(getOwner(), SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_BOSS_TIMER_02());
					scheduleDespawn(240);
					break;
				case 240:
					PacketSendUtility::broadcastToMap(getOwner(), SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_BOSS_TIMER_03());
					scheduleDespawn(60);
					break;
				case 60:
					getOwner().queueSkill(21534, 1, 3000);
					break;
			}
		}
	}, delayInSec * 1000);
}

// Java DynatoumAI.java:68-76
void DynatoumAI::onEndUseSkill(const skillengine::model::SkillTemplate* skillTemplate, int32_t skillLevel) {
	if (skillTemplate == nullptr) // Java: skillTemplate.getSkillId() on null
		throw runtime::NullPointerException("DynatoumAI: skill template");
	switch (skillTemplate->getSkillId()) {
		case 21534:
			getOwner().getController().delete_();
			PacketSendUtility::broadcastToMap(getOwner(), SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_BOSS_TIMER_04());
			break;
	}
}

// Java DynatoumAI.java:78-81
void DynatoumAI::cancelDespawnTask() {
	runtime::FutureRef task = despawnTask.get();
	if (task != nullptr && !task->isCancelled())
		task->cancel(true);
}

// Java DynatoumAI.java:83-87
void DynatoumAI::handleDespawned() {
	cancelDespawnTask();
	SummonerAI::handleDespawned();
}

// Java DynatoumAI.java:89-93
void DynatoumAI::handleDied() {
	cancelDespawnTask();
	SummonerAI::handleDied();
}

// Java DynatoumAI.java:95-98
void DynatoumAI::handleBackHome() {
	SummonerAI::handleBackHome();
}

} // namespace aion::gameserver::handlers::ai::instance::illuminaryObelisk
