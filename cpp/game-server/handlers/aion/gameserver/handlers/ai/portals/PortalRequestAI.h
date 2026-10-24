#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/portals/PortalAI.h"

namespace aion::gameserver::handlers::ai::portals {

/**
 * The AI of the portals that ask first ("portal_request"): the use asks to confirm the teleport to the teleporter's first location, at its
 * price; the answer teleports. Java's anonymous RequestResponseHandler is PortalRequestAI_Response of the .cpp.
 * <p>
 * Java: data/handlers/ai/portals/PortalRequestAI.java, @AIName("portal_request") (the marker is in the .cpp).
 *
 * @author xTz
 */
class PortalRequestAI : public PortalAI {
public:
	explicit PortalRequestAI(Npc& owner) : PortalAI(owner) {}

protected:
	void handleUseItemFinish(Player& player) override;
};

} // namespace aion::gameserver::handlers::ai::portals
