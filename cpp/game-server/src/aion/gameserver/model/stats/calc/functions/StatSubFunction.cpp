#include "aion/gameserver/model/stats/calc/functions/StatSubFunction.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::stats::calc::functions {

void StatSubFunction::apply(Stat2& statValue, const std::unordered_set<utils::stats::CalculationType>& calculationTypes) {
	AION_UNPORTED();
}

int32_t StatSubFunction::getPriority() {
	return isBonus() ? 60 : 30;
}

} // namespace aion::gameserver::model::stats::calc::functions
