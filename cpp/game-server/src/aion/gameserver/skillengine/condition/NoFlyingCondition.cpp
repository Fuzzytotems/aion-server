#include "aion/gameserver/skillengine/condition/NoFlyingCondition.h"

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/Skill.h"

namespace aion::gameserver::skillengine::condition {

bool NoFlyingCondition::validate(model::Skill& env) const {
	return (!env.getEffector()->isFlying());
}

bool NoFlyingCondition::validate(model::Effect& effect) const {
	return (!effect.getEffected()->isFlying());
}

} // namespace aion::gameserver::skillengine::condition
