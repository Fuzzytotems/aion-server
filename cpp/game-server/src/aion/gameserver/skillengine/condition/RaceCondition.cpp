#include "aion/gameserver/skillengine/condition/RaceCondition.h"

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/Skill.h"

namespace aion::gameserver::skillengine::condition {

using gameserver::model::Race;

namespace {

/** Java `for (Race race : races)` over the null `race` attribute throws */
const std::vector<Race>& requireRaces(const std::optional<std::vector<Race>>& races) {
	if (!races.has_value())
		throw runtime::NullPointerException("Cannot invoke \"java.util.List.iterator()\" because \"this.races\" is null");
	return *races;
}

} // namespace

bool RaceCondition::validate(model::Skill& env) const {
	if (!env.getFirstTarget() || !env.getEffector())
		return false;
	bool result = false;
	for (Race race : requireRaces(races)) {
		if (race == env.getFirstTarget()->getRace())
			result = true;
	}
	return result;
}

bool RaceCondition::validate(model::Effect& effect) const {
	if (!effect.getEffected() || !effect.getEffector())
		return false;
	bool result = false;
	for (Race race : requireRaces(races)) {
		if (race == effect.getEffected()->getRace())
			result = true;
	}
	return result;
}

} // namespace aion::gameserver::skillengine::condition
