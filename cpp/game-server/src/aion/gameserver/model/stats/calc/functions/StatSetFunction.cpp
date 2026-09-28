#include "aion/gameserver/model/stats/calc/functions/StatSetFunction.h"

#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::stats::calc::functions {

StatSetFunction::StatSetFunction(container::StatEnum name, int32_t valueValue) : StatFunction(name, valueValue, false) {
}

void StatSetFunction::apply(Stat2& statValue, const std::unordered_set<utils::stats::CalculationType>& /*calculationTypes*/) {
	if (isBonus())
		statValue.setBonus(static_cast<float>(getValue()));
	else
		statValue.setBase(static_cast<float>(getValue()));
}

int32_t StatSetFunction::getPriority() const {
	return isBonus() ? 70 : 40;
}

std::string StatSetFunction::toString() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::stats::calc::functions
