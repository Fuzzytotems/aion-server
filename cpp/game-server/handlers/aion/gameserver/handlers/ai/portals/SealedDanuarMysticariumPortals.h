#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/portals/PortalDialogAI.h"

namespace aion::gameserver::handlers::ai::portals {

/**
 * The AI of the Sealed Danuar Mysticarium portals ("sealed_danuar_mysticarium_portal"): the entrances (730721, 730722) take a key when no
 * instance is registered for the player and his cooldown allows; inside, 731583's first choice finishes its use.
 * <p>
 * Java: data/handlers/ai/portals/SealedDanuarMysticariumPortals.java, @AIName("sealed_danuar_mysticarium_portal") (the marker is in the .cpp).
 *
 * @author Yeats
 */
class SealedDanuarMysticariumPortals : public PortalDialogAI {
public:
	explicit SealedDanuarMysticariumPortals(Npc& owner) : PortalDialogAI(owner) {}

	bool onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) override;
};

} // namespace aion::gameserver::handlers::ai::portals
