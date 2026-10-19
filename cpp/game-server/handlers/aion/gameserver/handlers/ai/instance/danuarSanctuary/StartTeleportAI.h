#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

namespace aion::gameserver::handlers::ai::instance::danuarSanctuary {

/**
 * The Danuar Sanctuaries' start portals ("dsiportal"): each takes a key's holder to its way; without the key, the no-right page.
 * <p>
 * Java: data/handlers/ai/instance/danuarSanctuary/StartTeleportAI.java, @AIName("dsiportal") (the marker is in the .cpp).
 *
 * @author Cheatkiller
 */
class StartTeleportAI : public NpcAI {
public:
	explicit StartTeleportAI(Npc& owner) : NpcAI(owner) {}

protected:
	void handleDialogStart(Player& player) override;

public:
	bool onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) override;

private:
	void switchWay(Player& player);
};

} // namespace aion::gameserver::handlers::ai::instance::danuarSanctuary
