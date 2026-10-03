#include "aion/gameserver/model/stats/calc/Stat2.h"

#include <string>

#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/geoEngine/math/JavaFloat.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/model/templates/detail/JavaCasts.h"
#include "aion/gameserver/runtime/base/Unported.h"

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
	return "[" + std::string(xml::enumName(stat)) + " base=" + geoEngine::math::JavaFloat::toString(base) + ", bonus=" +
		geoEngine::math::JavaFloat::toString(bonus) + "]";
}

} // namespace aion::gameserver::model::stats::calc
