#include "aion/gameserver/skillengine/condition/OnFlyCondition.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::skillengine::condition {

bool OnFlyCondition::validate(model::Skill& /*env*/) const {
	AION_UNPORTED();
}

bool OnFlyCondition::validate(gameserver::model::stats::calc::Stat2& /*stat*/,
	gameserver::model::stats::calc::functions::IStatFunction& /*statFunction*/) const {
	AION_UNPORTED();
}

bool OnFlyCondition::validate(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

} // namespace aion::gameserver::skillengine::condition
