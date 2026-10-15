#include "aion/gameserver/services/siege/SiegeCounter.h"

#include <algorithm>
#include <vector>

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/siege/SiegeNpc.h"
#include "aion/gameserver/model/siege/SiegeRace.h"
#include "aion/gameserver/model/siege/SiegeRaceInfo.h"
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

// Java SiegeCounter.java:22-32
void SiegeCounter::addDamage(model::gameobjects::Creature& creature, int32_t damage) {
	model::siege::SiegeRace siegeRace;
	if (auto* player = dynamic_cast<model::gameobjects::player::Player*>(&creature))
		siegeRace = model::siege::getByRace(player->getRace());
	else if (auto* siegeNpc = dynamic_cast<model::gameobjects::siege::SiegeNpc*>(&creature))
		siegeRace = siegeNpc->getSiegeRace();
	else
		return;

	siegeRaceCounters.get(siegeRace)->addPoints(creature, damage);
}

// Java SiegeCounter.java:34-37
void SiegeCounter::addAbyssPoints(model::gameobjects::player::Player& player, int32_t ap) {
	model::siege::SiegeRace sr = model::siege::getByRace(player.getRace());
	siegeRaceCounters.get(sr)->addAbyssPoints(player, ap);
}

// Java SiegeCounter.java:39-41
runtime::Ptr<SiegeRaceCounter> SiegeCounter::getRaceCounter(model::siege::SiegeRace race) {
	return siegeRaceCounters.get(race);
}

// Java SiegeCounter.java:43-45
void SiegeCounter::addRaceDamage(model::siege::SiegeRace race, int32_t damage) {
	getRaceCounter(race)->addTotalDamage(damage);
}

// Java SiegeCounter.java:50-52: the counters with damage, sorted (compareTo: the highest total first; a stable sort over the EnumMap's
// ordinal order), the first - or the fallback race's counter
runtime::Ptr<SiegeRaceCounter> SiegeCounter::getWinnerRaceCounter(model::siege::SiegeRace fallbackRace) {
	std::vector<runtime::Ptr<SiegeRaceCounter>> counters;
	for (const runtime::Ptr<SiegeRaceCounter>& c : siegeRaceCounters.values())
		if (c->getTotalDamage() > 0)
			counters.push_back(c);
	std::stable_sort(counters.begin(), counters.end(), [](const auto& a, const auto& b) { return a->compareTo(*b) < 0; });
	return counters.empty() ? getRaceCounter(fallbackRace) : counters.front();
}

SiegeCounter::~SiegeCounter() = default;

} // namespace aion::gameserver::services::siege
