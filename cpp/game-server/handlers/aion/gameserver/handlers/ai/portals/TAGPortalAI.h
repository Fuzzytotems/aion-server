#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/portals/PortalDialogAI.h"

namespace aion::gameserver::handlers::ai::portals {

/**
 * The AI of the training grounds' portals ("t_a_g_portal"): the choices open the auto group window of the Discipline, Chaos or Harmony
 * Training Grounds; quest dialogs go to PortalDialogAI.
 * <p>
 * Java: data/handlers/ai/portals/TAGPortalAI.java, @AIName("t_a_g_portal") (the marker is in the .cpp).
 *
 * @author xTz
 */
class TAGPortalAI : public PortalDialogAI {
public:
	explicit TAGPortalAI(Npc& owner) : PortalDialogAI(owner) {}

	bool onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) override;
};

} // namespace aion::gameserver::handlers::ai::portals
