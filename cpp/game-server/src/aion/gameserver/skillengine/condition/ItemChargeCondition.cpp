#include "aion/gameserver/skillengine/condition/ItemChargeCondition.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::skillengine::condition {

bool ItemChargeCondition::validate(gameserver::model::stats::calc::Stat2& /*stat*/,
	gameserver::model::stats::calc::functions::IStatFunction& /*statFunction*/) const {
	AION_UNPORTED();
}

bool ItemChargeCondition::validate(model::Skill& /*env*/) const {
	return false;
}

} // namespace aion::gameserver::skillengine::condition
