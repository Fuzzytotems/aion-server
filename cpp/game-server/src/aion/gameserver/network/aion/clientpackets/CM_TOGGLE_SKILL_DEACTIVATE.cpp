#include "aion/gameserver/network/aion/clientpackets/CM_TOGGLE_SKILL_DEACTIVATE.h"

#include <string>

#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

using model::gameobjects::player::Player;

CM_TOGGLE_SKILL_DEACTIVATE::CM_TOGGLE_SKILL_DEACTIVATE(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_TOGGLE_SKILL_DEACTIVATE.java:23-27
void CM_TOGGLE_SKILL_DEACTIVATE::readImpl() {
	skillId = readUH();
	readH();
	readH();
}

// Java CM_TOGGLE_SKILL_DEACTIVATE.java:30-41
void CM_TOGGLE_SKILL_DEACTIVATE::runImpl() {
	const runtime::Ptr<Player> player = getConnection()->getActivePlayer();
	const skillengine::model::SkillTemplate* skillTemplate = dataholders::DataManager::SKILL_DATA->getSkillTemplate(skillId);
	if (skillTemplate == nullptr || (!skillTemplate->isToggle() && !skillTemplate->isStance())) {
		utils::audit::AuditLogger::log(*player,
			"tried to remove non-toggle skill effect (" + std::to_string(skillId) + ") through CM_TOGGLE_SKILL_DEACTIVATE");
		return;
	}
	player->getEffectController()->removeEffect(skillId);

	if (player->getController().getStanceSkillId() == skillId)
		player->getController().stopStance();
}

AION_CLIENT_PACKET(CM_TOGGLE_SKILL_DEACTIVATE);

} // namespace aion::gameserver::network::aion::clientpackets
