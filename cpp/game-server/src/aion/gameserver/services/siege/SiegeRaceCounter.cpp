#include "aion/gameserver/services/siege/SiegeRaceCounter.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::services::siege {

SiegeRaceCounter::SiegeRaceCounter(model::siege::SiegeRace value)
	: siegeRace(value) {
}

runtime::Ref<SiegeRaceCounter> SiegeRaceCounter::create(model::siege::SiegeRace value) {
	return runtime::makeRef<SiegeRaceCounter>(value);
}

void SiegeRaceCounter::addPoints(model::gameobjects::Creature& creature, int32_t damage) {
	AION_UNPORTED();
}

void SiegeRaceCounter::addTotalDamage(int32_t damage) {
	AION_UNPORTED();
}

void SiegeRaceCounter::addPlayerDamage(model::gameobjects::player::Player& player, int32_t damage) {
	AION_UNPORTED();
}

void SiegeRaceCounter::addAbyssPoints(model::gameobjects::player::Player& player, int32_t abyssPoints) {
	AION_UNPORTED();
}

int64_t SiegeRaceCounter::getTotalDamage() {
	AION_UNPORTED();
}

std::vector<std::pair<int32_t, int64_t>> SiegeRaceCounter::getPlayerDamageCounter() {
	AION_UNPORTED();
}

std::vector<std::pair<int32_t, int64_t>> SiegeRaceCounter::getPlayerAbyssPoints() {
	AION_UNPORTED();
}

int32_t SiegeRaceCounter::compareTo(const SiegeRaceCounter& o) const {
	AION_UNPORTED();
}

std::optional<int32_t> SiegeRaceCounter::getWinnerLegionId() {
	AION_UNPORTED();
}

SiegeRaceCounter::~SiegeRaceCounter() = default;

} // namespace aion::gameserver::services::siege
