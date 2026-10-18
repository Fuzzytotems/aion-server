#include "aion/gameserver/handlers/ai/instance/dragonLordsRefuge/GravityTornadoAI.h"

#include "aion/gameserver/ai/AIActions.h"

namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge {

AION_AI(GravityTornadoAI, "gravity_tornado");

// Java GravityTornadoAI.java:24-28
void GravityTornadoAI::handleSpawned() {
	NpcAI::handleSpawned();
	task.set(ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] { AIActions::useSkill(*this, getNpcId() == 283142 ? 20966 : 21901); },
		2500, 6000));
}

// Java GravityTornadoAI.java:30-34
void GravityTornadoAI::handleDespawned() {
	task.get()->cancel(true);
	NpcAI::handleDespawned();
}

// Java GravityTornadoAI.java:36-42
bool GravityTornadoAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::ALLOW_DECAY:
		case AIQuestion::ALLOW_RESPAWN:
		case AIQuestion::REWARD_AP_XP_DP_LOOT:
			return false;
		default:
			return NpcAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge
