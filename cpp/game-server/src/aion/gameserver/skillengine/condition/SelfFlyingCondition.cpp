#include "aion/gameserver/skillengine/condition/SelfFlyingCondition.h"

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/Skill.h"

namespace aion::gameserver::skillengine::condition {

using model::FlyingRestriction;

bool SelfFlyingCondition::validate(model::Skill& env) const {
	if (!env.getEffector())
		return false;
	switch (restriction) {
		case FlyingRestriction::FLY:
			return env.getEffector()->isInFlyingState();
		case FlyingRestriction::GROUND:
			return !env.getEffector()->isInFlyingState();
		default:
			break;
	}
	return true;
}

bool SelfFlyingCondition::validate(model::Effect& effect) const {
	if (!effect.getEffector())
		return false;
	switch (restriction) {
		case FlyingRestriction::FLY:
			return effect.getEffector()->isInFlyingState();
		case FlyingRestriction::GROUND:
			return !effect.getEffector()->isInFlyingState();
		default:
			break;
	}
	return true;
}

} // namespace aion::gameserver::skillengine::condition
