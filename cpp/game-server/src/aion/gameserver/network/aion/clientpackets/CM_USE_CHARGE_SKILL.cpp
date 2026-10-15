#include "aion/gameserver/network/aion/clientpackets/CM_USE_CHARGE_SKILL.h"

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

using model::gameobjects::player::Player;

CM_USE_CHARGE_SKILL::CM_USE_CHARGE_SKILL(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_USE_CHARGE_SKILL.java:21-23: the packet has no body
void CM_USE_CHARGE_SKILL::readImpl() {
}

// Java CM_USE_CHARGE_SKILL.java:25-32
void CM_USE_CHARGE_SKILL::runImpl() {
	const runtime::Ptr<Player> player = getConnection()->getActivePlayer();
	const runtime::Ptr<skillengine::model::Skill> chargeCastingSkill = player->getCastingSkill();
	if (chargeCastingSkill == nullptr || !chargeCastingSkill->getSkillTemplate()->isCharge())
		return;
	const int64_t chargeTimeMillis = commons::utils::currentTimeMillis() - chargeCastingSkill->getCastStartTime();
	player->getController().useChargeSkill(*chargeCastingSkill, chargeTimeMillis);
}

AION_CLIENT_PACKET(CM_USE_CHARGE_SKILL);

} // namespace aion::gameserver::network::aion::clientpackets
