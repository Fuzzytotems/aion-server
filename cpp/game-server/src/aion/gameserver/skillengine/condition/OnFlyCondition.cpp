#include "aion/gameserver/skillengine/condition/OnFlyCondition.h"

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/Skill.h"

namespace aion::gameserver::skillengine::condition {

bool OnFlyCondition::validate(model::Skill& env) const {
	return env.getEffector()->isInFlyingState();
}

bool OnFlyCondition::validate(gameserver::model::stats::calc::Stat2& stat,
	gameserver::model::stats::calc::functions::IStatFunction& /*statFunction*/) const {
	return stat.getOwner()->isInFlyingState();
}

bool OnFlyCondition::validate(model::Effect& effect) const {
	return effect.getEffected()->isInFlyingState();
}

} // namespace aion::gameserver::skillengine::condition
