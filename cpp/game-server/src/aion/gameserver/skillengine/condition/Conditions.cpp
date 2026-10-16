#include "aion/gameserver/skillengine/condition/Conditions.h"

namespace aion::gameserver::skillengine::condition {

bool Conditions::validate(model::Skill& skill) const {
	for (const std::unique_ptr<Condition>& condition : getConditions()) {
		if (!condition->validate(skill)) {
			return false;
		}
	}
	return true;
}

bool Conditions::canValidate(model::Skill& skill) const {
	for (const std::unique_ptr<Condition>& condition : getConditions()) {
		if (!condition->canValidate(skill)) {
			return false;
		}
	}
	return true;
}

bool Conditions::validate(gameserver::model::stats::calc::Stat2& stat, gameserver::model::stats::calc::functions::IStatFunction& statFunction) const {
	for (const std::unique_ptr<Condition>& condition : getConditions()) {
		if (!condition->validate(stat, statFunction)) {
			return false;
		}
	}
	return true;
}

bool Conditions::validate(model::Effect& effect) const {
	for (const std::unique_ptr<Condition>& condition : getConditions()) {
		if (!condition->validate(effect)) {
			return false;
		}
	}
	return true;
}

} // namespace aion::gameserver::skillengine::condition
