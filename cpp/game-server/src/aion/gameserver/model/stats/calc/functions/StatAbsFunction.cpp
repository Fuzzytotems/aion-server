#include "aion/gameserver/model/stats/calc/functions/StatAbsFunction.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::stats::calc::functions {

StatAbsFunction::StatAbsFunction(container::StatEnum name, int32_t valueValue, bool debuffValue) : StatFunction(name, valueValue, false) {
	this->debuff = debuffValue;
}

void StatAbsFunction::apply(Stat2& statValue, const std::unordered_set<utils::stats::CalculationType>& calculationTypes) {
	AION_UNPORTED();
}

int32_t StatAbsFunction::getPriority() const {
	if (debuff)
		return isBonus() ? 110 : 90;

	return isBonus() ? 100 : 80;
}

std::string StatAbsFunction::toString() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::stats::calc::functions
