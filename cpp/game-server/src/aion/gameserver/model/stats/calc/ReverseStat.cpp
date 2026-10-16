#include "aion/gameserver/model/stats/calc/ReverseStat.h"

namespace aion::gameserver::model::stats::calc {

ReverseStat::ReverseStat(container::StatEnum statValue, float baseValue, gameobjects::Creature& ownerValue) : Stat2(statValue, baseValue, ownerValue) {
}

void ReverseStat::addToBase(float value) {
	this->base -= value;
	if (this->base < 0) {
		this->base = 0;
	}
}

void ReverseStat::addToBonus(float value) {
	this->bonus -= value;
}

float ReverseStat::calculatePercent(int32_t delta) {
	float percent = (100 - delta) / 100.0f;
	// TODO need double check here for negatives
	return percent < 0 ? 0 : percent;
}

} // namespace aion::gameserver::model::stats::calc
