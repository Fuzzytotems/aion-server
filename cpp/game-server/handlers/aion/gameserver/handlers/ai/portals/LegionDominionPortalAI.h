#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/portals/PortalDialogAI.h"

namespace aion::gameserver::handlers::ai::portals {

/**
 * The AI of the Legion Dominion portal to Stonespear Reach ("legion_dominion_portal"): closed Wednesdays 8-10; level 65; an alliance (not
 * a league) whose captain with a dominion legion opens the instance (with a key if configured), the members enter it while their
 * cooldown allows.
 * <p>
 * Java: data/handlers/ai/portals/LegionDominionPortalAI.java, @AIName("legion_dominion_portal") (the marker is in the .cpp).
 *
 * @author Yeats
 */
class LegionDominionPortalAI : public PortalDialogAI {
public:
	explicit LegionDominionPortalAI(Npc& owner) : PortalDialogAI(owner) {}

	bool onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) override;

private:
	bool hasCd(Player& player);
};

} // namespace aion::gameserver::handlers::ai::portals
