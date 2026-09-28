#include "aion/gameserver/model/stats/calc/Stat2.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/templates/detail/JavaCasts.h"

namespace aion::gameserver::model::stats::calc {

using templates::detail::floatToInt;

Stat2::Stat2(container::StatEnum statValue, float baseValue, gameobjects::Creature& ownerValue) : stat(statValue), owner(ownerValue), base(baseValue) {
}

int32_t Stat2::getBase() {
	return floatToInt(base * this->getBaseRate());
}

int32_t Stat2::getBaseWithoutBaseRate() {
	return floatToInt(base);
}

int32_t Stat2::getBonus() {
	return floatToInt(bonus);
}

int32_t Stat2::getCurrent() {
	return floatToInt(getExactCurrent());
}

float Stat2::getExactCurrent() {
	return (base * baseRate + bonus * bonusRate + base * fixedBonusRate) * finalRate;
}

float Stat2::getExactCurrentWithoutBonus() {
	return (base * baseRate + base * fixedBonusRate) * finalRate;
}

float Stat2::getExactCurrentWithoutFixedBonus() {
	return (base * baseRate + bonus * bonusRate) * finalRate;
}

std::string Stat2::toString() const {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::stats::calc
