#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/ActionItemNpcAI.h"

namespace aion::gameserver::handlers::ai::portals::abyssalsplinter {

/**
 * The AI of the Abyssal Splinter's teleportation devices ("teleportation_device"): each of the three devices (by its x) sends the user to
 * its own place.
 * <p>
 * Java: data/handlers/ai/portals/abyssalsplinter/AbyssalSplinterPortalAI.java, @AIName("teleportation_device") (the marker is in the .cpp).
 *
 * @author Ritsu
 */
class AbyssalSplinterPortalAI : public ActionItemNpcAI {
public:
	explicit AbyssalSplinterPortalAI(Npc& owner) : ActionItemNpcAI(owner) {}

protected:
	void handleUseItemFinish(Player& player) override;
};

} // namespace aion::gameserver::handlers::ai::portals::abyssalsplinter
