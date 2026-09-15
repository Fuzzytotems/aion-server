#include "aion/gameserver/skillengine/condition/WeaponCondition.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::skillengine::condition {

bool WeaponCondition::validate(model::Skill& /*env*/) const {
	AION_UNPORTED();
}

bool WeaponCondition::validate(gameserver::model::stats::calc::Stat2& /*stat*/,
	gameserver::model::stats::calc::functions::IStatFunction& /*statFunction*/) const {
	AION_UNPORTED();
}

} // namespace aion::gameserver::skillengine::condition
