#include "aion/gameserver/model/stats/calc/AdditionStat.h"

namespace aion::gameserver::model::stats::calc {

AdditionStat::AdditionStat(container::StatEnum statValue, float baseValue, gameobjects::Creature& ownerValue) : Stat2(statValue, baseValue, ownerValue) {
}

void AdditionStat::addToBase(float value) {
	this->base += value;
}

void AdditionStat::addToBonus(float value) {
	this->bonus += value;
}

float AdditionStat::calculatePercent(int32_t delta) {
	return (100 + delta) / 100.0f;
}

} // namespace aion::gameserver::model::stats::calc
