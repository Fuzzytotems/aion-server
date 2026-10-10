#include "aion/gameserver/handlers/ai/ShifterAI.h"

#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"


namespace aion::gameserver::handlers::ai {

AION_AI(ShifterAI, "shifter");

// Java ShifterAI.java:20-24
void ShifterAI::handleUseItemFinish(Player& player) {
	ActionItemNpcAI::handleUseItemFinish(player);
	PacketSendUtility::broadcastPacket(player, SM_EMOTION(getOwner(), EmotionType::EMOTE, 144, 0), true);
}

} // namespace aion::gameserver::handlers::ai
