#include "aion/gameserver/handlers/ai/instance/tiamatStrongHold/GravityAI.h"

#include "aion/gameserver/ai/AIActions.h"

namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold {

AION_AI(GravityAI, "gravity");

// Java GravityAI.java:24-29
void GravityAI::handleSpawned() {
	NpcAI::handleSpawned();
	task.set(ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] { AIActions::useSkill(*this, 20738); }, 0, 3250));
	ThreadPoolManager::getInstance().schedule({this}, [this] { AIActions::deleteOwner(*this); }, 20000);
}

// Java GravityAI.java:31-36
void GravityAI::handleDespawned() {
	runtime::FutureRef current = task.get();
	if (current != nullptr)
		current->cancel(true);
	NpcAI::handleDespawned();
}

// Java GravityAI.java:38-44
bool GravityAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::ALLOW_DECAY:
		case AIQuestion::ALLOW_RESPAWN:
		case AIQuestion::REWARD_AP_XP_DP_LOOT:
			return false;
		default:
			return NpcAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold
