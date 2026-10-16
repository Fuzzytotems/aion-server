#include "aion/gameserver/handlers/ai/instance/drakenspire/WaveEntrySensorAI.h"

#include "aion/gameserver/controllers/NpcController.h"

namespace aion::gameserver::handlers::ai::instance::drakenspire {

AION_AI(WaveEntrySensorAI, "wave_entry_sensor");

// Java WaveEntrySensorAI.java:20-25
void WaveEntrySensorAI::handleCreatureDetected(Creature& creature) {
	AggressiveNpcAI::handleCreatureDetected(creature);
	if (dynamic_cast<Player*>(&creature) != nullptr)
		getOwner().getController().delete_();
}

} // namespace aion::gameserver::handlers::ai::instance::drakenspire
