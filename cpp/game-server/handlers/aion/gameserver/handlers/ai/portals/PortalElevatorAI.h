#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/portals/PortalAI.h"

namespace aion::gameserver::handlers::ai::portals {

/**
 * The AI of the elevator portals ("portal_elevator"): the portal plays emote 144 to the user and his watchers, then ports as a PortalAI.
 * <p>
 * Java: data/handlers/ai/portals/PortalElevatorAI.java, @AIName("portal_elevator") (the marker is in the .cpp).
 *
 * @author xTz
 */
class PortalElevatorAI : public PortalAI {
public:
	explicit PortalElevatorAI(Npc& owner) : PortalAI(owner) {}

protected:
	void handleUseItemFinish(Player& player) override;
};

} // namespace aion::gameserver::handlers::ai::portals
