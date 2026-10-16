#include "aion/gameserver/skillengine/condition/TargetCondition.h"

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/skillengine/properties/FirstTargetAttribute.h"
#include "aion/gameserver/skillengine/properties/Properties.h"
#include "aion/gameserver/skillengine/properties/TargetRangeAttribute.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::skillengine::condition {

using gameserver::model::gameobjects::Npc;
using gameserver::model::gameobjects::player::Player;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using properties::FirstTargetAttribute;
using properties::TargetRangeAttribute;
using runtime::Ptr;

bool TargetCondition::validate(model::Skill& skill) const {
	if (value == TargetAttribute::NONE || value == TargetAttribute::ALL)
		return true;
	// Java: getTargetType().equals(AREA) dereferences a missing target_type
	std::optional<TargetRangeAttribute> targetType = skill.getSkillTemplate()->getProperties()->getTargetType();
	if (!targetType.has_value())
		throw runtime::NullPointerException(
			"Cannot invoke \"TargetRangeAttribute.equals(Object)\" because the return value of \"Properties.getTargetType()\" is null");
	if (*targetType == TargetRangeAttribute::AREA)
		return true;
	if (skill.getSkillTemplate()->getProperties()->getFirstTarget() != FirstTargetAttribute::TARGET
		&& skill.getSkillTemplate()->getProperties()->getFirstTarget() != FirstTargetAttribute::TARGETORME)
		return true;
	if (skill.getSkillTemplate()->getProperties()->getFirstTarget() == FirstTargetAttribute::TARGETORME && skill.getEffector() == skill.getFirstTarget())
		return true;
	bool result = false;
	switch (value) {
		case TargetAttribute::NPC:
			result = static_cast<bool>(runtime::as<Npc>(skill.getFirstTarget()));
			break;
		case TargetAttribute::PC:
			result = static_cast<bool>(runtime::as<Player>(skill.getFirstTarget()));
			break;
		default:
			break;
	}
	if (!result) {
		if (Ptr<Player> player = runtime::as<Player>(skill.getEffector()))
			utils::PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_SKILL_TARGET_IS_NOT_VALID());
	}
	return result;
}

} // namespace aion::gameserver::skillengine::condition
