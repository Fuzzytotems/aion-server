#include "aion/gameserver/skillengine/effect/BackDashEffect.h"

#include <cstdint>

#include "aion/gameserver/geoEngine/math/Vector3f.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/model/DashStatus.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/geo/GeoService.h"

namespace aion::gameserver::skillengine::effect {

void BackDashEffect::calculate(model::Effect& effect) const {
	effect.setDashStatus(model::DashStatus::BACKDASH);
	runtime::Ptr<gameserver::model::gameobjects::Creature> effector = effect.getEffector();
	int8_t h = utils::PositionUtil::getHeadingTowards(*effector, *effect.getEffected());
	float inverseAngle = utils::PositionUtil::convertHeadingToAngle(h) + 180; // flip by 180 degrees for opposite direction
	geoEngine::math::Vector3f closestCollision = world::geo::GeoService::getInstance().findMovementCollision(*effector, inverseAngle, distance);
	effect.getSkill()->setTargetPosition(closestCollision.getX(), closestCollision.getY(), closestCollision.getZ(), h);
	world::World::getInstance().updatePosition(*effector, closestCollision.getX(), closestCollision.getY(), closestCollision.getZ(), h);
	DamageEffect::calculate(effect);
}

} // namespace aion::gameserver::skillengine::effect
