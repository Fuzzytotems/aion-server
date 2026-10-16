#include "aion/gameserver/handlers/ai/worlds/panesterra/Rank100CorridorAI.h"

#include "aion/gameserver/model/gameobjects/player/AbyssRank.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"

namespace aion::gameserver::handlers::ai::worlds::panesterra {

AION_AI(Rank100CorridorAI, "rank_100_advance_corridor");

// Java Rank100CorridorAI.java:17-20
Rank100CorridorAI::Rank100CorridorAI(Npc& owner) : AdvanceCorridorAI(owner) {
	despawnInMin = 5;
}

// Java Rank100CorridorAI.java:22-29
void Rank100CorridorAI::handleDialogStart(Player& player) {
	if (static_cast<int32_t>(player.getAbyssRank()->getRank()) < static_cast<int32_t>(AbyssRankEnum::STAR5_OFFICER)) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_TELEPOTER_GAB1_USER03());
		return;
	}
	AdvanceCorridorAI::handleDialogStart(player);
}

} // namespace aion::gameserver::handlers::ai::worlds::panesterra
