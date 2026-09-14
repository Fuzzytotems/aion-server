#include "aion/gameserver/model/stats/calc/Stat2.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::stats::calc {

Stat2::Stat2(container::StatEnum statValue, float baseValue, gameobjects::Creature& ownerValue) : stat(statValue), owner(ownerValue), base(baseValue) {
}

int32_t Stat2::getBase() {
	AION_UNPORTED();
}

int32_t Stat2::getBaseWithoutBaseRate() {
	AION_UNPORTED();
}

int32_t Stat2::getBonus() {
	AION_UNPORTED();
}

int32_t Stat2::getCurrent() {
	AION_UNPORTED();
}

float Stat2::getExactCurrent() {
	AION_UNPORTED();
}

float Stat2::getExactCurrentWithoutBonus() {
	AION_UNPORTED();
}

float Stat2::getExactCurrentWithoutFixedBonus() {
	AION_UNPORTED();
}

std::string Stat2::toString() const {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::stats::calc
