#include "aion/gameserver/handlers/ai/instance/beshmundirTemple/IsbariyaServantsAI.h"

#include "aion/gameserver/ai/AIActions.h"

namespace aion::gameserver::handlers::ai::instance::beshmundirTemple {

AION_AI(IsbariyaServantsAI, "isbariyaServants");

// Java IsbariyaServantsAI.java:20-25
void IsbariyaServantsAI::handleSpawned() {
	AggressiveNpcAI::handleSpawned();
	int32_t lifetime = (getNpcId() == 281659 ? 20000 : 10000);
	toDespawn(lifetime);
}

// Java IsbariyaServantsAI.java:27-35
void IsbariyaServantsAI::toDespawn(int32_t delay) {
	ThreadPoolManager::getInstance().schedule({this}, [this] { AIActions::deleteOwner(*this); }, delay);
}

} // namespace aion::gameserver::handlers::ai::instance::beshmundirTemple
