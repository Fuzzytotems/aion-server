#include "aion/gameserver/model/stats/calc/functions/StatRateFunction.h"

#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::stats::calc::functions {

StatRateFunction::StatRateFunction(container::StatEnum name, int32_t valueValue, bool bonusValue) : StatFunction(name, valueValue, bonusValue) {
}

void StatRateFunction::apply(Stat2& statValue, const std::unordered_set<utils::stats::CalculationType>& /*calculationTypes*/) {
	if (isBonus()) {
		int32_t baseValue = statValue.getBaseWithoutBaseRate();
		if (getName() == container::StatEnum::SPEED && getValue() < 0 && statValue.getBonus() < 0) { // fix to avoid run speed <= 0%
			// calculate relative to current resultValue if negative, otherwise we end up with <= 0% on multiple stat functions
			baseValue = statValue.getCurrent();
		}
		// Java: baseValue * getValue() is an int product (wraps), then / 100f
		statValue.addToBonus(static_cast<int32_t>(static_cast<uint32_t>(baseValue) * static_cast<uint32_t>(getValue())) / 100.0f);
	} else {
		statValue.setBase(statValue.getBaseWithoutBaseRate() * statValue.calculatePercent(getValue()));
	}
}

int32_t StatRateFunction::getPriority() const {
	return isBonus() ? 50 : 20;
}

std::string StatRateFunction::toString() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::stats::calc::functions
