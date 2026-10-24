#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"

namespace aion::gameserver::handlers::ai::events {

/**
 * The AI of the Rainbow Snake event npcs ("rainbow_snake"): a player who comes into sight is called once; the first choice closes the
 * dialog and casts the day's two buffs.
 * <p>
 * Java: data/handlers/ai/events/RainbowSnakeAI.java, @AIName("rainbow_snake") (the marker is in the .cpp).
 *
 * @author (none)
 */
class RainbowSnakeAI : public GeneralNpcAI {
private:
	/** Java: ConcurrentHashMap.newKeySet() - TODO remove + fix NpcShoutsService */
	runtime::ConcurrentKeySet<int32_t> messagedPlayers{AION_LOCK_CLASS(RainbowSnakeAI::messagedPlayers#stripe)};

public:
	explicit RainbowSnakeAI(Npc& npc) : GeneralNpcAI(npc) {}

	void handleCreatureDetected(Creature& creature) override;

protected:
	void handleCreatureNotSee(Creature& creature) override;

public:
	bool onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) override;
};

} // namespace aion::gameserver::handlers::ai::events
