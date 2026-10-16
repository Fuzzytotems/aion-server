#include "aion/gameserver/services/siege/SiegeRaceCounter.h"

#include <algorithm>

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/TemporaryPlayerTeam.h"
#include "aion/gameserver/model/team/alliance/PlayerAlliance.h"
#include "aion/gameserver/model/team/league/League.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::services::siege {

SiegeRaceCounter::SiegeRaceCounter(model::siege::SiegeRace value)
	: siegeRace(value) {
}

runtime::Ref<SiegeRaceCounter> SiegeRaceCounter::create(model::siege::SiegeRace value) {
	return runtime::makeRef<SiegeRaceCounter>(value);
}

// Java SiegeRaceCounter.java:40-44
void SiegeRaceCounter::addPoints(model::gameobjects::Creature& creature, int32_t damage) {
	addTotalDamage(damage);
	if (auto* player = dynamic_cast<model::gameobjects::player::Player*>(&creature))
		addPlayerDamage(*player, damage);
}

// Java SiegeRaceCounter.java:46-48
void SiegeRaceCounter::addTotalDamage(int32_t damage) {
	totalDamage.addAndGet(damage);
}

// Java SiegeRaceCounter.java:50-52
void SiegeRaceCounter::addPlayerDamage(model::gameobjects::player::Player& player, int32_t damage) {
	addToCounter(player.getObjectId(), damage, playerDamageCounter);
}

// Java SiegeRaceCounter.java:54-56
void SiegeRaceCounter::addAbyssPoints(model::gameobjects::player::Player& player, int32_t abyssPoints) {
	addToCounter(player.getObjectId(), abyssPoints, playerAPCounter);
}

// Java SiegeRaceCounter.java:81-83
int64_t SiegeRaceCounter::getTotalDamage() {
	return totalDamage.get();
}

// Java SiegeRaceCounter.java:90-92
std::vector<std::pair<int32_t, int64_t>> SiegeRaceCounter::getPlayerDamageCounter() {
	return getOrderedCounterMap(playerDamageCounter);
}

// Java SiegeRaceCounter.java:99-101
std::vector<std::pair<int32_t, int64_t>> SiegeRaceCounter::getPlayerAbyssPoints() {
	return getOrderedCounterMap(playerAPCounter);
}

// Java SiegeRaceCounter.java:119-122
int32_t SiegeRaceCounter::compareTo(const SiegeRaceCounter& o) const {
	const int64_t mine = totalDamage.get();
	const int64_t theirs = o.totalDamage.get();
	return theirs < mine ? -1 : (theirs == mine ? 0 : 1); // Java: Long.compare(o.getTotalDamage(), getTotalDamage())
}

// Java SiegeRaceCounter.java:133-171
std::optional<int32_t> SiegeRaceCounter::getWinnerLegionId() {
	using model::gameobjects::player::Player;
	// Java: a HashMap<Player, AtomicLong>; C++: the leaders in the order they are first met, with their summed damage
	std::vector<std::pair<runtime::Ptr<Player>, int64_t>> teamDamageMap;
	const auto addDamage = [&teamDamageMap](const runtime::Ptr<Player>& leader, int64_t damage) {
		auto entry = std::find_if(teamDamageMap.begin(), teamDamageMap.end(), [&](const auto& e) { return e.first.get() == leader.get(); });
		if (entry == teamDamageMap.end())
			teamDamageMap.emplace_back(leader, damage);
		else
			entry->second += damage;
	};
	for (int32_t id : playerDamageCounter.keySet()) {
		runtime::Ptr<Player> player = world::World::getInstance().getPlayer(id);

		if (player) {
			if (player->getCurrentTeam()) {
				if (!player->isInLeague()) {
					runtime::Ptr<Player> teamLeader = player->getCurrentTeam()->getLeaderObject();
					const int64_t damage = playerDamageCounter.get(id)->get();
					if (teamLeader)
						addDamage(teamLeader, damage);
				} else {
					runtime::Ptr<Player> teamLeader = player->getPlayerAlliance()->getLeague()->getLeaderObject()->getLeaderObject();
					const int64_t damage = playerDamageCounter.get(id)->get();
					if (teamLeader)
						addDamage(teamLeader, damage);
				}
			} else { // solo
				const int64_t damage = playerDamageCounter.get(id)->get();
				addDamage(player, damage);
			}
		}
	}
	if (teamDamageMap.empty())
		return std::nullopt;

	// Java: getOrderedCounterMap(teamDamageMap).keySet().iterator().next() - the leader with the most damage (a positive total; with only
	// zero totals Java's iterator().next() throws NoSuchElementException, as here). Equal totals keep this order, not Java's hash order
	// (docs/deviations/P5-12a.md)
	std::stable_sort(teamDamageMap.begin(), teamDamageMap.end(), [](const auto& o1, const auto& o2) { return o2.second < o1.second; });
	if (teamDamageMap.front().second <= 0)
		throw runtime::NoSuchElementException("no team damage above zero"); // Java: iterator().next() of the empty ordered map
	runtime::Ptr<Player> topTeamLeader = teamDamageMap.front().first;
	runtime::Ptr<model::team::legion::Legion> legion = topTeamLeader->getLegion();

	return legion ? std::optional<int32_t>(legion->getLegionId()) : std::nullopt;
}

SiegeRaceCounter::~SiegeRaceCounter() = default;

} // namespace aion::gameserver::services::siege
