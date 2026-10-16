#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/ActionItemNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::linkgateFoundry {

/**
 * The AI of Linkgate Foundry's secret room teleporter ("linkgateFoundrySecretRoomTeleport"): its use opens the dialog, whose three
 * choices teleport to the three floors.
 * <p>
 * Java: data/handlers/ai/instance/linkgateFoundry/LinkgateFoundrySecretTeleportAI.java, @AIName("linkgateFoundrySecretRoomTeleport")
 * (the marker is in the .cpp).
 *
 * @author Cheatkiller
 */
class LinkgateFoundrySecretTeleportAI : public ActionItemNpcAI {
public:
	explicit LinkgateFoundrySecretTeleportAI(Npc& owner) : ActionItemNpcAI(owner) {}

protected:
	void handleUseItemFinish(Player& player) override;

public:
	bool onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) override;

private:
	void switchFloor(Player& player, int32_t dialogActionId);
};

} // namespace aion::gameserver::handlers::ai::instance::linkgateFoundry
