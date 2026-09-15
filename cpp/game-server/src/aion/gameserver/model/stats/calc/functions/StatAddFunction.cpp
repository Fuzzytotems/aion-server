#include "aion/gameserver/model/stats/calc/functions/StatAddFunction.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::stats::calc::functions {

StatAddFunction::StatAddFunction(container::StatEnum name, int32_t valueValue, bool bonusValue) : StatFunction(name, valueValue, bonusValue) {
}

void StatAddFunction::apply(Stat2& statValue, const std::unordered_set<utils::stats::CalculationType>& calculationTypes) {
	AION_UNPORTED();
}

int32_t StatAddFunction::getPriority() const {
	return isBonus() ? 60 : 30;
}

std::string StatAddFunction::toString() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::stats::calc::functions
