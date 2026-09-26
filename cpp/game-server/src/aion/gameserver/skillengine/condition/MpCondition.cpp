#include "aion/gameserver/skillengine/condition/MpCondition.h"

#include "aion/commons/utils/Exception.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_LOG.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_TYPE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::skillengine::condition {

using gameserver::model::gameobjects::player::Player;
using network::aion::serverpackets::SM_ATTACK_STATUS_LOG;
using network::aion::serverpackets::SM_ATTACK_STATUS_TYPE;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using runtime::Ptr;

bool MpCondition::validate(model::Skill& skill) const {
	if (!canValidate(skill))
		return false;
	skill.getEffector()->getLifeStats()->reduceMp(SM_ATTACK_STATUS_TYPE::USED_MP, getCost(skill), 0, SM_ATTACK_STATUS_LOG::REGULAR);
	return true;
}

bool MpCondition::canValidate(model::Skill& skill) const {
	// npcs have no mp, so they must not be blocked by an mp cost
	if (Ptr<Player> player = runtime::as<Player>(skill.getEffector()); player && player->getLifeStats()->getCurrentMp() < getCost(skill)) {
		utils::PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_SKILL_NOT_ENOUGH_MP());
		return false;
	}
	return true;
}

int32_t MpCondition::getCost(model::Skill& skill) const {
	int32_t valueWithDelta = value + delta * skill.getSkillLevel();
	if (ratio)
		valueWithDelta = (skill.getEffector()->getLifeStats()->getMaxMp() * valueWithDelta) / 100;
	int32_t changeMpPercent = skill.getBoostSkillCost();
	if (changeMpPercent != 0) {
		// changeMpPercent is negative
		int32_t divisor = 100 / changeMpPercent;
		if (divisor == 0) // Java: integer division by zero throws (a boost beyond +-100 %), C++ would be undefined
			throw commons::utils::ArithmeticException("/ by zero");
		valueWithDelta = valueWithDelta - ((valueWithDelta / divisor));
	}
	return valueWithDelta;
}

} // namespace aion::gameserver::skillengine::condition
