#include "aion/gameserver/skillengine/effect/SummonTotemEffect.h"

#include <cmath>
#include <cstdint>

#include "aion/gameserver/geoEngine/collision/CollisionIntention.h"
#include "aion/gameserver/geoEngine/collision/CollisionIntentionInfo.h"
#include "aion/gameserver/geoEngine/collision/IgnoreProperties.h"
#include "aion/gameserver/geoEngine/math/Vector3f.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/NpcObjectType.h"
#include "aion/gameserver/model/gameobjects/Servant.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/geo/GeoService.h"

namespace aion::gameserver::skillengine::effect {

namespace {

/** Java Math.toRadians(double) (JDK 9+: angdeg * DEGREES_TO_RADIANS) */
constexpr double toRadians(double angdeg) noexcept {
	return angdeg * 0.017453292519943295;
}

} // namespace

void SummonTotemEffect::applyEffect(model::Effect& effect) const {
	gameserver::model::gameobjects::Creature& effector = *effect.getEffector();
	float x = effector.getX();
	float y = effector.getY();
	float z = effector.getZ();
	if (effect.getSkill()->isFirstTargetSelf()) {
		gameserver::model::gameobjects::Creature& effected = *effect.getEffected();
		double radian = toRadians(utils::PositionUtil::convertHeadingToAngle(effect.getEffector()->getHeading()));
		geoEngine::math::Vector3f pos = world::geo::GeoService::getInstance().getClosestCollision(effector,
			effected.getX() + static_cast<float>(std::cos(radian) * 2), effected.getY() + static_cast<float>(std::sin(radian) * 2), effected.getZ(),
			true, getId(geoEngine::collision::CollisionIntention::DEFAULT_COLLISIONS),
			*geoEngine::collision::IgnoreProperties::of(effector.getRace()));
		x = pos.getX();
		y = pos.getY();
		z = pos.getZ();
	} else if (effect.getSkill()->isPointSkill()) { // fix for [657]Battle Banner
		x = effect.getX();
		y = effect.getY();
		z = effect.getZ();
		if (x == 0 && y == 0) {
			x = effector.getX();
			y = effector.getY();
			z = effector.getZ();
		}
	}
	int32_t spawnDuration = time;
	const std::string& group = effect.getSkillTemplate()->getGroup(); // Java null: "" (equals no group name)
	if (group == "PR_PROVOKESERVENT") {
		spawnDuration = 20; // Taunting Spirit should stay 20s but the client says only 15s
	} else if (group == "FI_WARFLAG") {
		spawnDuration = 15; // same here Battle Banner 7s -> 15s
	}
	spawnServant(effect, spawnDuration, gameserver::model::gameobjects::NpcObjectType::TOTEM, x, y, z);
}

} // namespace aion::gameserver::skillengine::effect
