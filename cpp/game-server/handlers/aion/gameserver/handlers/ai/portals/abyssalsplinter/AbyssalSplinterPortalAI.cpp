#include "aion/gameserver/handlers/ai/portals/abyssalsplinter/AbyssalSplinterPortalAI.h"

#include "aion/gameserver/services/teleport/TeleportService.h"

namespace aion::gameserver::handlers::ai::portals::abyssalsplinter {

AION_AI(AbyssalSplinterPortalAI, "teleportation_device");

// Java AbyssalSplinterPortalAI.java:20-29
void AbyssalSplinterPortalAI::handleUseItemFinish(Player& player) {
	float x = getOwner().getX();
	if (x == 302.201f)
		TeleportService::teleportTo(player, getOwner().getWorldId(), 294.632f, 732.189f, 215.854f);
	else if (x == 334.001f)
		TeleportService::teleportTo(player, getOwner().getWorldId(), 338.475f, 701.417f, 215.916f);
	else if (x == 362.192f)
		TeleportService::teleportTo(player, getOwner().getWorldId(), 373.611f, 739.125f, 215.903f);
}

} // namespace aion::gameserver::handlers::ai::portals::abyssalsplinter
