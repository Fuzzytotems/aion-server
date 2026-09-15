#include "aion/gameserver/model/stats/calc/functions/StatRateFunction.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::stats::calc::functions {

StatRateFunction::StatRateFunction(container::StatEnum name, int32_t valueValue, bool bonusValue) : StatFunction(name, valueValue, bonusValue) {
}

void StatRateFunction::apply(Stat2& statValue, const std::unordered_set<utils::stats::CalculationType>& calculationTypes) {
	AION_UNPORTED();
}

int32_t StatRateFunction::getPriority() const {
	return isBonus() ? 50 : 20;
}

std::string StatRateFunction::toString() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::stats::calc::functions
