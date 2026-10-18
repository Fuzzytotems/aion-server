#include "aion/gameserver/handlers/ai/instance/dragonLordsRefuge/TiamatDragonAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"

namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge {

AION_AI(TiamatDragonAI, "tiamat_dragon");

// Java TiamatDragonAI.java:24-30
void TiamatDragonAI::handleSpawned() {
	AggressiveNpcAI::handleSpawned();
	ThreadPoolManager::getInstance().schedule({this}, [this] { AIActions::useSkill(*this, 20920); }, 4000);
	ThreadPoolManager::getInstance().schedule({this}, [this] { getOwner().queueSkill(20984, 1); }, 300000);
}

// Java TiamatDragonAI.java:32-45
void TiamatDragonAI::onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) {
	switch (skillTemplate->getSkillId()) {
		case 20920:
			AIActions::useSkill(*this, 20975); // Fissure Buff
			AIActions::useSkill(*this, 20976); // Wrath Buff
			AIActions::useSkill(*this, 20977); // Gravity Buff
			AIActions::useSkill(*this, 20978); // Petrification Buff
			break;
		case 20984:
			PacketSendUtility::broadcastToMap(getOwner(), SM_SYSTEM_MESSAGE::STR_IDTIAMAT_TIAMAT_WARNING_MSG());
			break;
	}
}

// Java TiamatDragonAI.java:47-52
bool TiamatDragonAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::REWARD_AP_XP_DP_LOOT:
			return false;
		default:
			return AggressiveNpcAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge
