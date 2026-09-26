#include "aion/gameserver/services/siege/SiegeCounter.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/siege/SiegeRace.h"
#include "aion/gameserver/services/siege/SiegeRaceCounter.h"

namespace aion::gameserver::services::siege {

SiegeCounter::SiegeCounter() {
	siegeRaceCounters.put(model::siege::SiegeRace::ELYOS, SiegeRaceCounter::create(model::siege::SiegeRace::ELYOS));
	siegeRaceCounters.put(model::siege::SiegeRace::ASMODIANS, SiegeRaceCounter::create(model::siege::SiegeRace::ASMODIANS));
	siegeRaceCounters.put(model::siege::SiegeRace::BALAUR, SiegeRaceCounter::create(model::siege::SiegeRace::BALAUR));
}

runtime::Ref<SiegeCounter> SiegeCounter::create() {
	return runtime::makeRef<SiegeCounter>();
}

void SiegeCounter::addDamage(model::gameobjects::Creature& creature, int32_t damage) {
	AION_UNPORTED();
}

void SiegeCounter::addAbyssPoints(model::gameobjects::player::Player& player, int32_t ap) {
	AION_UNPORTED();
}

runtime::Ptr<SiegeRaceCounter> SiegeCounter::getRaceCounter(model::siege::SiegeRace race) {
	AION_UNPORTED();
}

void SiegeCounter::addRaceDamage(model::siege::SiegeRace race, int32_t damage) {
	AION_UNPORTED();
}

runtime::Ptr<SiegeRaceCounter> SiegeCounter::getWinnerRaceCounter(model::siege::SiegeRace fallbackRace) {
	AION_UNPORTED();
}

SiegeCounter::~SiegeCounter() = default;

} // namespace aion::gameserver::services::siege
