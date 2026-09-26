#include "aion/gameserver/skillengine/effect/DashEffect.h"

#include <cmath>
#include <cstdint>
#include <numbers>
#include <string>

#include "aion/gameserver/geoEngine/math/Vector3f.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/templates/BoundRadius.h"
#include "aion/gameserver/model/templates/VisibleObjectTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/model/DashStatus.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/geo/GeoService.h"

namespace aion::gameserver::skillengine::effect {

namespace {

/** Java Math.toRadians(double) (JDK 9+: angdeg * DEGREES_TO_RADIANS) */
constexpr double toRadians(double angdeg) noexcept {
	return angdeg * 0.017453292519943295;
}

/**
 * Java `creature.getObjectTemplate().getBoundRadius()` dereferenced: a PlayerCommonData whose radius was never set answers null, and Java's
 * getMaxOfFrontAndSide() call on it is a NullPointerException (a plain C++ dereference of nullptr is undefined)
 */
const gameserver::model::templates::BoundRadius& boundRadiusOf(gameserver::model::gameobjects::Creature& creature) {
	const gameserver::model::templates::BoundRadius* radius = creature.getObjectTemplate()->getBoundRadius();
	if (radius == nullptr)
		throw runtime::NullPointerException("getBoundRadius() of " + std::to_string(creature.getObjectId()) + " is null");
	return *radius;
}

} // namespace

void DashEffect::calculate(model::Effect& effect) const {
	runtime::Ptr<gameserver::model::gameobjects::Creature> effected = effect.getEffected();
	// Java `effected.equals(firstTarget)`: AionObject.equals compares the objectIds; a null effected throws, a null first target is not equal
	gameserver::model::gameobjects::Creature& self = *effected;
	runtime::Ptr<gameserver::model::gameobjects::Creature> firstTarget = effect.getSkill()->getFirstTarget();
	if (firstTarget && self.equals(*firstTarget)) { // move only once for Dash-AoE (e.g 2705)
		effect.setDashStatus(model::DashStatus::DASH);
		int8_t h = utils::PositionUtil::getHeadingTowards(*effect.getEffector(), *effected);
		double radian = toRadians(utils::PositionUtil::convertHeadingToAngle(h));
		float distance = boundRadiusOf(*effect.getEffector()).getMaxOfFrontAndSide() + boundRadiusOf(*effected).getMaxOfFrontAndSide() + 1;
		const float x1 = static_cast<float>(std::cos(std::numbers::pi + radian)) * distance;
		const float y1 = static_cast<float>(std::sin(std::numbers::pi + radian)) * distance;
		geoEngine::math::Vector3f closestCollision = world::geo::GeoService::getInstance().getClosestCollision(*effect.getEffected(),
			effected->getX() + x1, effected->getY() + y1, effected->getZ());
		effect.getSkill()->setTargetPosition(closestCollision.getX(), closestCollision.getY(), closestCollision.getZ(), h);
		world::World::getInstance().updatePosition(*effect.getEffector(), closestCollision.getX(), closestCollision.getY(), closestCollision.getZ(),
			h);
	}
	DamageEffect::calculate(effect);
}

} // namespace aion::gameserver::skillengine::effect
