#pragma once

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/siege/fwd.h"
#include "aion/gameserver/services/siege/fwd.h"

namespace aion::gameserver::services::siege {

/**
 * A class that contains all the counters for the siege. One SiegeCounter per race should be used.
 *
 * Java implements Comparable<SiegeRaceCounter>.
 *
 * @author SoulKeeper
 */
class SiegeRaceCounter : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	runtime::AtomicLong totalDamage{AION_LOCK_CLASS(SiegeRaceCounter::totalDamage)}; // Java: = new AtomicLong()
	// Java: = new ConcurrentHashMap<>()
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<runtime::Rc<runtime::AtomicLong>>> playerDamageCounter{
		AION_LOCK_CLASS(SiegeRaceCounter::playerDamageCounter#stripe)};
	// Java: = new ConcurrentHashMap<>()
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<runtime::Rc<runtime::AtomicLong>>> playerAPCounter{
		AION_LOCK_CLASS(SiegeRaceCounter::playerAPCounter#stripe)};
	const model::siege::SiegeRace siegeRace;

protected:
	explicit SiegeRaceCounter(model::siege::SiegeRace siegeRace);

public:
	static runtime::Ref<SiegeRaceCounter> create(model::siege::SiegeRace value);

	void addPoints(model::gameobjects::Creature& creature, int32_t damage);

	void addTotalDamage(int32_t damage);

	void addPlayerDamage(model::gameobjects::player::Player& player, int32_t damage);

	void addAbyssPoints(model::gameobjects::player::Player& player, int32_t abyssPoints);

protected:
	/** Java generic method (hub-headers.md §8.3); synchronizes on this when it creates a counter */
	template <class K>
	void addToCounter(K key, int32_t value, runtime::ConcurrentHashMap<K,
		runtime::Ref<runtime::Rc<runtime::AtomicLong>>>& counterMap) { AION_UNPORTED(); }

public:
	int64_t getTotalDamage();

	/** Returns "playerId to damage" map. Map is ordered by damage in "descending" order */
	/** Java: a LinkedHashMap in descending order of the counters; C++: its entries in that order */
	std::vector<std::pair<int32_t, int64_t>> getPlayerDamageCounter();

	/** Returns "player to abyss points" map. Map is ordered by abyssPoints in descending order */
	/** Java: a LinkedHashMap in descending order of the counters; C++: its entries in that order */
	std::vector<std::pair<int32_t, int64_t>> getPlayerAbyssPoints();

protected:
	/** Java generic method: a LinkedHashMap ordered by the counters in descending order, so the entries in that order (hub-headers.md §7.1) */
	template <class K>
	std::vector<std::pair<K, int64_t>> getOrderedCounterMap(runtime::ConcurrentHashMap<K,
		runtime::Ref<runtime::Rc<runtime::AtomicLong>>>& unorderedMap) { AION_UNPORTED(); }

public:
	int32_t compareTo(const SiegeRaceCounter& o) const;

	model::siege::SiegeRace getSiegeRace() const { return this->siegeRace; }

	/** Returns Legion of the Leader of the strongest Team */
	std::optional<int32_t> getWinnerLegionId();

protected:
	~SiegeRaceCounter() override;
};

} // namespace aion::gameserver::services::siege
