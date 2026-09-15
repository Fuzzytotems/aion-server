#include "aion/gameserver/dataholders/MotionData.h"

#include <algorithm>

#include "aion/gameserver/dataholders/detail/JavaHashMapOrder.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/skillengine/model/Motion.h"
#include "aion/gameserver/skillengine/model/MotionTime.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/skillengine/model/Times.h"

namespace aion::gameserver::dataholders {

using skillengine::model::MotionTime;

void MotionData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	detail::JavaHashMapOrder<std::string, const MotionTime*> order;
	for (const MotionTime& motion : motionTimes) {
		motionTimesMap.insert_or_assign(motion.getName(), &motion);
		order.put(motion.getName(), &motion, detail::javaHashCode(std::string_view(motion.getName())));
	}
	motionTimesInHashOrder = order.values();
	// Java: motionTimes = null (the C++ index points into the storage, which stays)
}

const std::vector<const MotionTime*>& MotionData::getMotionTimes() const {
	return motionTimesInHashOrder;
}

const MotionTime* MotionData::getMotionTime(std::string_view name) const {
	auto it = motionTimesMap.find(name);
	return it != motionTimesMap.end() ? it->second : nullptr;
}

const MotionTime* MotionData::getMotionTime(skillengine::model::Skill& skill) const {
	const skillengine::model::Motion* motion = skill.getSkillTemplate()->getMotion();
	if (motion == nullptr || motion->getName().empty()) // instant skills like Remove Shock (283) or Feint (912)
		return nullptr;                                   // some skills, like Blind Side (3467) or scroll/food buffs have no motion
	return getMotionTime(motion->getName());
}

float MotionData::calculateAnimationTimeUntilFirstHit(model::gameobjects::player::Player& player, skillengine::model::Skill& skill) const {
	const MotionTime* motionTime = getMotionTime(skill);
	if (motionTime == nullptr)
		return 0.0f;
	const skillengine::model::Times* times = motionTime->getTimesFor(player, 1);
	if (times == nullptr)
		return 0.0f;
	int32_t motionSpeed = skill.getSkillTemplate()->getMotion()->getSpeed() * 10;
	float attackRate = player.getGameStats()->getAttackSpeedRate();
	float motionSpeedRate = player.isHitTimeBoosted() ? std::min(attackRate, calculateCastSpeedRate(player.getHitTimeBoostCastSpeed())) : attackRate;
	return (player.isInRobotMode() ? times->getAnimationLength() : times->getMinTime()) * static_cast<float>(motionSpeed) * motionSpeedRate;
}

std::optional<MotionData::AnimationTimes> MotionData::calculateAnimationTimesAfterLastHit(model::gameobjects::player::Player& /*player*/,
                                                                                          skillengine::model::Skill& /*skill*/) const {
	// needs `skill instanceof ChargeSkill chargeSkill ? chargeSkill.getMotionId()`: skillengine/model/ChargeSkill.h (P5-02) does not exist yet
	AION_UNPORTED();
}

float MotionData::calculateCastSpeedRate(float castSpeedForAnimationBoost) {
	castSpeedForAnimationBoost = std::max(0.5f, std::min(1.0f, castSpeedForAnimationBoost)); // these are limits enforced by the game client
	return castSpeedForAnimationBoost + (1 - castSpeedForAnimationBoost) / 2;                // only half of the cast speed can affect animations
}

int32_t MotionData::size() const {
	return static_cast<int32_t>(motionTimesMap.size());
}

} // namespace aion::gameserver::dataholders
