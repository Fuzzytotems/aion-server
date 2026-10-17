#include "aion/gameserver/handlers/ai/instance/rakes/SuspiciousCannonAI.h"

#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"

namespace aion::gameserver::handlers::ai::instance::rakes {

AION_AI(SuspiciousCannonAI, "suspiciouscannon");

// Java SuspiciousCannonAI.java:23-30
void SuspiciousCannonAI::handleUseItemFinish(Player& player) {
	int32_t teleportId = getOwner().getNpcId() == 730769 ? 247001 : 73001;
	player.setState(CreatureState::FLYING);
	player.unsetState(CreatureState::ACTIVE);
	player.setFlightTeleportId(teleportId);
	PacketSendUtility::sendPacket(player, SM_EMOTION(player, EmotionType::START_FLYTELEPORT, teleportId, 0));
}

} // namespace aion::gameserver::handlers::ai::instance::rakes
