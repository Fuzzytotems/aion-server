#include "aion/gameserver/handlers/ai/instance/tiamatStrongHold/FireStormAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/controllers/NpcController.h"

namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold {

AION_AI(FireStormAI, "tahabatafirestorm");

// Java FireStormAI.java:24-32
void FireStormAI::handleSpawned() {
	NpcAI::handleSpawned();
	const int32_t skill = getNpcId() == 283102 ? 20753 : 20759; // 4.0
	task.set(ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this, skill] { AIActions::useSkill(*this, skill); }, 0, 1000));

	if (getNpcId() != 283102) // 4.0
		despawn();
}

// Java FireStormAI.java:34-36
void FireStormAI::despawn() {
	ThreadPoolManager::getInstance().schedule({this}, [this] { getOwner().getController().delete_(); }, 20000);
}

// Java FireStormAI.java:38-42
void FireStormAI::handleDespawned() {
	task.get()->cancel(true);
	NpcAI::handleDespawned();
}

// Java FireStormAI.java:44-50
bool FireStormAI::ask(AIQuestion question) {
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
