#include "aion/gameserver/handlers/ai/instance/dragonLordsRefuge/ThickDustAI.h"

#include "aion/gameserver/ai/AIActions.h"

namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge {

AION_AI(ThickDustAI, "thick_dust");

// Java ThickDustAI.java:19-23
void ThickDustAI::handleSpawned() {
	NpcAI::handleSpawned();
	ThreadPoolManager::getInstance().schedule({this}, [this] { AIActions::deleteOwner(*this); }, 10000);
}

} // namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge
