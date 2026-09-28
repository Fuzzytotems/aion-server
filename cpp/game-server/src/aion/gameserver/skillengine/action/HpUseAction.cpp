#include "aion/gameserver/skillengine/action/HpUseAction.h"

#include <limits>

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_LOG.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_TYPE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::skillengine::action {

using gameserver::model::gameobjects::Creature;
using gameserver::model::gameobjects::player::Player;
using network::aion::serverpackets::SM_ATTACK_STATUS_LOG;
using network::aion::serverpackets::SM_ATTACK_STATUS_TYPE;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using runtime::Ptr;

namespace {

/** Java `(int) f`: NaN 0, saturating (a C++ cast is undefined out of range) - Skill.cpp's and MotionData.cpp's helper */
int32_t javaFloatToInt(float value) {
	if (value != value)
		return 0;
	if (value >= 2147483648.0f)
		return std::numeric_limits<int32_t>::max();
	if (value <= -2147483648.0f)
		return std::numeric_limits<int32_t>::min();
	return static_cast<int32_t>(value);
}

} // namespace

bool HpUseAction::act(model::Skill& skill) const {
	if (!canAct(skill))
		return false;
	Ptr<Creature> effector = skill.getEffector();
	// npcs pass the check even when they cannot afford it and then pay what they have, down to 1 hp
	effector->getLifeStats()->reduceHp(SM_ATTACK_STATUS_TYPE::USED_HP, getCost(skill), 0, SM_ATTACK_STATUS_LOG::REGULAR, *effector);
	return true;
}

bool HpUseAction::canAct(model::Skill& skill) const {
	// npcs are never blocked by an hp cost, they pay what they have, see validate()
	// the cast may never be lethal
	if (Ptr<Player> player = runtime::as<Player>(skill.getEffector()); player && player->getLifeStats()->getCurrentHp() <= getCost(skill)) {
		utils::PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_SKILL_NOT_ENOUGH_HP());
		return false;
	}
	return true;
}

int32_t HpUseAction::getCost(model::Skill& skill) const {
	int32_t valueWithDelta = value + delta * skill.getSkillLevel();
	if (ratio) // Java: (int) (valueWithDelta / 100f * maxHp) - float arithmetic, truncated toward zero, saturating beyond the int range
		valueWithDelta = javaFloatToInt(static_cast<float>(valueWithDelta) / 100.0f * static_cast<float>(skill.getEffector()->getLifeStats()->getMaxHp()));
	return valueWithDelta;
}

} // namespace aion::gameserver::skillengine::action
