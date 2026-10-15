#include "aion/gameserver/handlers/ai/instance/kamarBf/KamarTeleportAI.h"

#include <string>
#include <vector>

#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"

namespace aion::gameserver::handlers::ai::instance::kamarBf {

AION_AI(KamarTeleportAI, "kamar_teleport");

// Java KamarTeleportAI.java:23-27
void KamarTeleportAI::handleSpawned() {
	portals::PortalAI::handleSpawned();
	remainingUses = 25;
}

// Java KamarTeleportAI.java:29-40
void KamarTeleportAI::handleUseItemFinish(Player& player) {
	if (remainingUses.get() <= 0) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE(1500906, std::vector<std::string>{}));
		return;
	}
	portals::PortalAI::handleUseItemFinish(player);
	remainingUses = remainingUses.get() - 1;
	if (remainingUses.get() == 0)
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE(1500906, std::vector<std::string>{}));
	else
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE(1500905, remainingUses.get()));
}

} // namespace aion::gameserver::handlers::ai::instance::kamarBf
