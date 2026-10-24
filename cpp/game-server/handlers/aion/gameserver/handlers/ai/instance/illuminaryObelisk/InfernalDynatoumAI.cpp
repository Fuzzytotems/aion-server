#include "aion/gameserver/handlers/ai/instance/illuminaryObelisk/InfernalDynatoumAI.h"

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::illuminaryObelisk {

AION_AI(InfernalDynatoumAI, "infernal_dynatoum");

// Java InfernalDynatoumAI.java:20-43
void InfernalDynatoumAI::scheduleDespawn(int32_t delayInSec) {
	despawnTask = ThreadPoolManager::getInstance().schedule({this}, [this, delayInSec] {
		if (!isDead()) {
			switch (delayInSec) {
				case 1:
					PacketSendUtility::broadcastToMap(getOwner(), SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_HARD_BOSS_TIMER_01());
					scheduleDespawn(2);
					break;
				case 2:
					PacketSendUtility::broadcastToMap(getOwner(), SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_HARD_BOSS_TIMER_02());
					scheduleDespawn(240);
					break;
				case 240:
					PacketSendUtility::broadcastToMap(getOwner(), SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_HARD_BOSS_TIMER_03());
					scheduleDespawn(60);
					break;
				case 60:
					getOwner().queueSkill(21534, 1, 3000);
					break;
			}
		}
	}, delayInSec * 1000);
}

// Java InfernalDynatoumAI.java:45-48 (Java has no @Override here, but it overrides)
void InfernalDynatoumAI::removeBossEntries() {
	PacketSendUtility::broadcastToMap(getOwner(), SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_HARD_BOSS_PORTAL_DESTROY());
	for (const runtime::Ptr<Npc>& p : getPosition()->getWorldMapInstance()->getNpcs({702216})) {
		if (p != nullptr)
			p->getController().delete_();
	}
}

// Java InfernalDynatoumAI.java:50-58
void InfernalDynatoumAI::onEndUseSkill(const skillengine::model::SkillTemplate* skillTemplate, int32_t skillLevel) {
	if (skillTemplate == nullptr) // Java: skillTemplate.getSkillId() on null
		throw runtime::NullPointerException("InfernalDynatoumAI: skill template");
	switch (skillTemplate->getSkillId()) {
		case 21534:
			getOwner().getController().delete_();
			PacketSendUtility::broadcastToMap(getOwner(), SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_HARD_BOSS_TIMER_04());
			break;
	}
}

} // namespace aion::gameserver::handlers::ai::instance::illuminaryObelisk
