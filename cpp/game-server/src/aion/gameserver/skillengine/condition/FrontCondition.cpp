#include "aion/gameserver/skillengine/condition/FrontCondition.h"

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/utils/PositionUtil.h"

namespace aion::gameserver::skillengine::condition {

bool FrontCondition::validate(model::Skill& env) const {
	if (!env.getFirstTarget() || !env.getEffector())
		return false;
	return utils::PositionUtil::isInFrontOf(*env.getEffector(), *env.getFirstTarget());
}

bool FrontCondition::validate(model::Effect& effect) const {
	if (!effect.getEffected() || !effect.getEffector())
		return false;
	return utils::PositionUtil::isInFrontOf(*effect.getEffector(), *effect.getEffected());
}

} // namespace aion::gameserver::skillengine::condition
