#include "aion/gameserver/model/stats/calc/functions/StatSubFunction.h"

#include "aion/gameserver/model/stats/calc/Stat2.h"

namespace aion::gameserver::model::stats::calc::functions {

void StatSubFunction::apply(Stat2& statValue, const std::unordered_set<utils::stats::CalculationType>& /*calculationTypes*/) {
	// Java -getValue() (int negation wraps)
	int32_t negated = static_cast<int32_t>(0u - static_cast<uint32_t>(getValue()));
	if (isBonus()) {
		statValue.addToBonus(static_cast<float>(negated));
	} else {
		statValue.addToBase(static_cast<float>(negated));
	}
}

int32_t StatSubFunction::getPriority() const {
	return isBonus() ? 60 : 30;
}

} // namespace aion::gameserver::model::stats::calc::functions
