#include "aion/gameserver/skillengine/properties/FirstTargetRangeProperty.h"

#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/controllers/movement/CreatureMoveController.h"
#include "aion/gameserver/geoEngine/collision/IgnoreProperties.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/CreatureGameStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/skillengine/properties/FirstTargetAttribute.h"
#include "aion/gameserver/skillengine/properties/Properties.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/geo/GeoService.h"

namespace aion::gameserver::skillengine::properties {

using gameserver::model::gameobjects::Creature;
using gameserver::model::gameobjects::player::Player;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using runtime::Ptr;
using utils::PacketSendUtility;
using utils::PositionUtil;
using world::geo::GeoService;

bool FirstTargetRangeProperty::set(model::Skill& skill, const Properties* properties, Properties_CastState castState) {
	float firstTargetRange = static_cast<float>(properties->getFirstTargetRange());
	if (!skill.isFirstTargetRangeCheck())
		return true;

	Ptr<Creature> effector = skill.getEffector();
	Ptr<Creature> firstTarget = skill.getFirstTarget();

	if (properties->getFirstTarget() == FirstTargetAttribute::POINT) {
		if (!GeoService::getInstance().canSee(*effector, skill.getX(), skill.getY(), skill.getZ(),
				*geoEngine::collision::IgnoreProperties::of(effector->getRace()))) {
			if (Ptr<Player> player = runtime::as<Player>(effector))
				PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_SKILL_OBSTACLE());
			return false;
		}
		return true;
	}

	if (!firstTarget)
		return false;

	if (firstTarget->equals(*effector))
		return true;

	// NPCs don't cancel skills once started, could be abused -> no range or geo to check
	if (castState != Properties_CastState::CAST_START && !runtime::as<Player>(effector))
		return true;

	// on end cast check add revision distance value (only for pvp targets, checked on 4.6 PTS)
	if (castState == Properties_CastState::CAST_END && runtime::as<Player>(firstTarget->getMaster()))
		firstTargetRange += static_cast<float>(properties->getRevisionDistance());

	// Add Weapon Range to distance
	if (properties->isAddWeaponRange())
		firstTargetRange += static_cast<float>(effector->getGameStats()->getAttackRange()->getCurrent()) / 1000.0f;

	// fixes first hit sometimes incorrectly not going through
	if (effector->getMoveController()->isInMove() && !firstTarget->getAggroList().isHating(*effector))
		firstTargetRange += PositionUtil::calculateMaxCoveredDistance(*effector, 50);

	if (!firstTarget->getEffectController()->isInAnyAbnormalState(effect::AbnormalState::CANT_MOVE_STATE)
		&& !PositionUtil::isInAttackRange(effector, firstTarget, firstTargetRange)) {
		if (Ptr<Player> player = runtime::as<Player>(effector))
			PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_SKILL_NOT_ENOUGH_DISTANCE());
		return false;
	}

	// TODO check for all targets too
	if (!GeoService::getInstance().canSee(*effector, *firstTarget)) {
		if (Ptr<Player> player = runtime::as<Player>(effector))
			PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_SKILL_OBSTACLE());
		return false;
	}
	return true;
}

} // namespace aion::gameserver::skillengine::properties
