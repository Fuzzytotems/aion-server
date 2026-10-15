#include "aion/gameserver/skillengine/condition/HpCondition.h"

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

using gameserver::model::gameobjects::Creature;
using gameserver::model::gameobjects::player::Player;
using network::aion::serverpackets::SM_ATTACK_STATUS_LOG;
using network::aion::serverpackets::SM_ATTACK_STATUS_TYPE;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using runtime::Ptr;

bool HpCondition::validate(model::Skill& skill) const {
	if (!canValidate(skill))
		return false;
	Ptr<Creature> effector = skill.getEffector();
	// npcs pass the check even when they cannot afford it and then pay what they have, down to 1 hp (example: skillId 18304)
	effector->getLifeStats()->reduceHp(SM_ATTACK_STATUS_TYPE::USED_HP, getCost(skill), 0, SM_ATTACK_STATUS_LOG::REGULAR, *effector);
	return true;
}

bool HpCondition::canValidate(model::Skill& skill) const {
	// npcs are never blocked by an hp cost, they pay what they have, see validate()
	// the cast may never be lethal
	if (Ptr<Player> player = runtime::as<Player>(skill.getEffector()); player && player->getLifeStats()->getCurrentHp() <= getCost(skill)) {
		utils::PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_SKILL_NOT_ENOUGH_HP());
		return false;
	}
	return true;
}

int32_t HpCondition::getCost(model::Skill& skill) const {
	int32_t valueWithDelta = value + delta * skill.getSkillLevel();
	if (ratio)
		valueWithDelta = (skill.getEffector()->getLifeStats()->getMaxHp() * valueWithDelta) / 100;
	return valueWithDelta;
}

} // namespace aion::gameserver::skillengine::condition
