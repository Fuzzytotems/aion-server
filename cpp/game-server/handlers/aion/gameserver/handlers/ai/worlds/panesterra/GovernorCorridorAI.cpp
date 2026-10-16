#include "aion/gameserver/handlers/ai/worlds/panesterra/GovernorCorridorAI.h"

#include "aion/gameserver/model/gameobjects/player/AbyssRank.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"

namespace aion::gameserver::handlers::ai::worlds::panesterra {

AION_AI(GovernorCorridorAI, "governor_advance_corridor");

// Java GovernorCorridorAI.java:17-20
GovernorCorridorAI::GovernorCorridorAI(Npc& owner) : AdvanceCorridorAI(owner) {
	despawnInMin = 5;
}

// Java GovernorCorridorAI.java:22-29
void GovernorCorridorAI::handleDialogStart(Player& player) {
	if (player.getAbyssRank()->getRank() != AbyssRankEnum::SUPREME_COMMANDER) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_TELEPOTER_GAB1_USER05());
		return;
	}
	AdvanceCorridorAI::handleDialogStart(player);
}

} // namespace aion::gameserver::handlers::ai::worlds::panesterra
