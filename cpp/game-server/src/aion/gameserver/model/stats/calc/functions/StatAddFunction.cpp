#include "aion/gameserver/model/stats/calc/functions/StatAddFunction.h"

#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::stats::calc::functions {

StatAddFunction::StatAddFunction(container::StatEnum name, int32_t valueValue, bool bonusValue) : StatFunction(name, valueValue, bonusValue) {
}

void StatAddFunction::apply(Stat2& statValue, const std::unordered_set<utils::stats::CalculationType>& /*calculationTypes*/) {
	if (isBonus()) {
		statValue.addToBonus(static_cast<float>(getValue()));
	} else {
		statValue.addToBase(static_cast<float>(getValue()));
	}
}

int32_t StatAddFunction::getPriority() const {
	return isBonus() ? 60 : 30;
}

std::string StatAddFunction::toString() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::stats::calc::functions
