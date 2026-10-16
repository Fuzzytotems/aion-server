#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"

namespace aion::gameserver::handlers::ai::worlds {

/**
 * The world blessers ("world_blesser"): Netalion, Renniah, Erdat and Karzanke talk as general npcs, any other blesser shows page 1011; their
 * SETPRO1 casts Blessing of Growth (20950) on the player, a quest selection opens the quest's page.
 * <p>
 * Java: data/handlers/ai/worlds/WorldBlesserAI.java, @AIName("world_blesser") (the marker is in the .cpp).
 *
 * @author xTz, bobobear
 */
class WorldBlesserAI : public GeneralNpcAI {
public:
	explicit WorldBlesserAI(Npc& owner) : GeneralNpcAI(owner) {}

protected:
	void handleDialogStart(Player& player) override;

public:
	bool onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) override;
};

} // namespace aion::gameserver::handlers::ai::worlds
