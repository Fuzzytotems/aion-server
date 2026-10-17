#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/OneDmgAI.h"

#include <optional>
#include <vector>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/collections/HashSet.h"

namespace aion::gameserver::handlers::ai::events {

/**
 * The AI of the Halloween pumpkins ("halloween_pumpkin"): a third of the small ones turn into a large pumpkin (219484) at the spawn; 10
 * hp, never thinks; a large one may drop Fine Bracing Water (once a day per account) or a weapon prototype of a looter's class.
 * <p>
 * Java: data/handlers/ai/events/HalloweenPumpkinAI.java, @AIName("halloween_pumpkin") (the marker is in the .cpp).
 *
 * @author (none)
 */
class HalloweenPumpkinAI : public OneDmgAI {
private:
	/** Java: private static final Map<Integer, Long> nextRewardMillisByAccountId = new ConcurrentHashMap<>() */
	static runtime::ConcurrentHashMap<int32_t, int64_t>& nextRewardMillisByAccountId();

public:
	explicit HalloweenPumpkinAI(Npc& owner) : OneDmgAI(owner) {}

protected:
	void handleSpawned() override;

public:
	bool canThink() override;

	void modifyOwnerStat(Stat2& stat) override;

protected:
	void handleDropRegistered() override;

private:
	bool canReward(Player& player);

	void replaceDropWithItem(int32_t itemId);

	void tryReplaceDropWithWeaponPrototype();

	std::optional<int32_t> selectWeaponPrototypeItemId(runtime::RcHashSet<int32_t>& allowedLooters);

	std::vector<int32_t> getWeaponPrototypeItemIds(Player& player);

	void replaceWithLargePumpkin();
};

} // namespace aion::gameserver::handlers::ai::events
