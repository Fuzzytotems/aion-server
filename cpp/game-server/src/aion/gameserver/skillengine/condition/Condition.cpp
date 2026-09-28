#include "aion/gameserver/skillengine/condition/Condition.h"

namespace aion::gameserver::skillengine::condition {

bool Condition::canValidate(model::Skill& /*skill*/) const {
	return true;
}

bool Condition::validate(gameserver::model::stats::calc::Stat2& /*stat*/,
	gameserver::model::stats::calc::functions::IStatFunction& /*statFunction*/) const {
	return true;
}

bool Condition::validate(model::Effect& /*effect*/) const {
	return true;
}

} // namespace aion::gameserver::skillengine::condition
