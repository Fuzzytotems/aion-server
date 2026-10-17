#include "aion/gameserver/handlers/ai/instance/drakenspire/WaveEventBomberAI.h"

namespace aion::gameserver::handlers::ai::instance::drakenspire {

AION_AI(WaveEventBomberAI, "wave_event_bomber");

// Java WaveEventBomberAI.java:19-23
void WaveEventBomberAI::handleSpawned() {
	GeneralNpcAI::handleSpawned();
	PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(getOwner()), 1501312, 4000);
}

} // namespace aion::gameserver::handlers::ai::instance::drakenspire
