#include "aion/gameserver/handlers/ai/siege/MineAI.h"

#include "aion/gameserver/ai/AIActions.h"

namespace aion::gameserver::handlers::ai::siege {

AION_AI(MineAI, "siege_mine");

// Java MineAI.java:19-29
void MineAI::handleCreatureAggro(Creature& creature) {
	static_cast<void>(creature);
	AIActions::useSkill(*this, 18407);
	ThreadPoolManager::getInstance().schedule({this}, [this] { AIActions::deleteOwner(*this); }, 1500);
}

} // namespace aion::gameserver::handlers::ai::siege
