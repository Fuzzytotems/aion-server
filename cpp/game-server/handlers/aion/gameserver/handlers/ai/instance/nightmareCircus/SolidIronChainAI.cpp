#include "aion/gameserver/handlers/ai/instance/nightmareCircus/SolidIronChainAI.h"

#include "aion/gameserver/network/aion/serverpackets/SM_PLAY_MOVIE.h"

namespace aion::gameserver::handlers::ai::instance::nightmareCircus {

AION_AI(SolidIronChainAI, "solidironchain");

// Java SolidIronChainAI.java:20-23
bool SolidIronChainAI::canThink() {
	return false;
}

// Java SolidIronChainAI.java:25-29
void SolidIronChainAI::handleDespawned() {
	AggressiveNpcAI::handleDespawned();
	PacketSendUtility::broadcastToMap(getOwner(), SM_PLAY_MOVIE(false, 0, 0, 983, true));
}

} // namespace aion::gameserver::handlers::ai::instance::nightmareCircus
