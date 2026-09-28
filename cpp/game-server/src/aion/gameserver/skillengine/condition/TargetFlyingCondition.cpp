#include "aion/gameserver/skillengine/condition/TargetFlyingCondition.h"

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/Skill.h"

namespace aion::gameserver::skillengine::condition {

using model::FlyingRestriction;

bool TargetFlyingCondition::validate(model::Skill& env) const {
	if (!env.getFirstTarget())
		return false;
	switch (restriction) {
		case FlyingRestriction::FLY:
			return env.getFirstTarget()->isFlying();
		case FlyingRestriction::GROUND:
			return !env.getFirstTarget()->isFlying();
		default:
			break;
	}
	return true;
}

bool TargetFlyingCondition::validate(model::Effect& effect) const {
	if (!effect.getEffected())
		return false;
	switch (restriction) {
		case FlyingRestriction::FLY:
			return effect.getEffected()->isFlying();
		case FlyingRestriction::GROUND:
			return !effect.getEffected()->isFlying();
		default:
			break;
	}
	return true;
}

} // namespace aion::gameserver::skillengine::condition
