#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/ActionItemNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::linkgateFoundry {

/**
 * The AI of Linkgate Foundry's boss portal ("linkgateFoundryBossTeleport"): its use opens the dialog; the chosen boss level takes all the
 * keys (at least 1, 5 or 7), swaps the portal, spawns Belsagos' chosen form (with a guardian below level 4) and teleports the player in.
 * <p>
 * Java: data/handlers/ai/instance/linkgateFoundry/LinkgateFoundryBossTeleportAI.java, @AIName("linkgateFoundryBossTeleport")
 * (the marker is in the .cpp).
 *
 * @author Cheatkiller
 */
class LinkgateFoundryBossTeleportAI : public ActionItemNpcAI {
public:
	explicit LinkgateFoundryBossTeleportAI(Npc& owner) : ActionItemNpcAI(owner) {}

protected:
	void handleUseItemFinish(Player& player) override;

public:
	bool onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) override;

private:
	void selectBoss(Player& player, int32_t dialogActionId);
};

} // namespace aion::gameserver::handlers::ai::instance::linkgateFoundry
