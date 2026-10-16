#include "aion/gameserver/handlers/ai/portals/PortalElevatorAI.h"

#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"

namespace aion::gameserver::handlers::ai::portals {

AION_AI(PortalElevatorAI, "portal_elevator");

// Java PortalElevatorAI.java:20-24
void PortalElevatorAI::handleUseItemFinish(Player& player) {
	PacketSendUtility::broadcastPacket(player, network::aion::serverpackets::SM_EMOTION(getOwner(), EmotionType::EMOTE, 144, 0), true);
	PortalAI::handleUseItemFinish(player);
}

} // namespace aion::gameserver::handlers::ai::portals
