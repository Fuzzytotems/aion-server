#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

namespace aion::gameserver::handlers::ai::instance::aturamSkyFortress {

/**
 * The AI of Hariken's supply chest ("hariken_supply_chest"): its first choice gives the Talon Summoning Device and the Bottomless
 * Bucket to a player who has neither.
 * <p>
 * Java: data/handlers/ai/instance/aturamSkyFortress/HarikenSupplyChestAI.java, @AIName("hariken_supply_chest") (the marker is in the .cpp).
 *
 * @author cheatkiller
 */
class HarikenSupplyChestAI : public NpcAI {
public:
	explicit HarikenSupplyChestAI(Npc& owner) : NpcAI(owner) {}

protected:
	void handleDialogStart(Player& player) override;

public:
	bool onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) override;

private:
	void addItems(Player& player);
};

} // namespace aion::gameserver::handlers::ai::instance::aturamSkyFortress
