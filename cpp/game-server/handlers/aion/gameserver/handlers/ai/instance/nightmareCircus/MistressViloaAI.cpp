#include "aion/gameserver/handlers/ai/instance/nightmareCircus/MistressViloaAI.h"


namespace aion::gameserver::handlers::ai::instance::nightmareCircus {

AION_AI(MistressViloaAI, "mistressviloa");

// Java MistressViloaAI.java:20-24
void MistressViloaAI::handleSpawned() {
	SummonerAI::handleSpawned();
	PacketSendUtility::broadcastMessage(getOwner(), 1501135, 3000);
}

// Java MistressViloaAI.java:26-29
void MistressViloaAI::handleSpawnFinished(const model::templates::ai::SummonGroup& summonGroup) {
	PacketSendUtility::broadcastMessage(getOwner(), 1501134, 1000);
}

} // namespace aion::gameserver::handlers::ai::instance::nightmareCircus
