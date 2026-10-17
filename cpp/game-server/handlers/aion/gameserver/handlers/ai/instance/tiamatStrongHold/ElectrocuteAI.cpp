#include "aion/gameserver/handlers/ai/instance/tiamatStrongHold/ElectrocuteAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/controllers/NpcController.h"

namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold {

AION_AI(ElectrocuteAI, "electrocute");

// Java ElectrocuteAI.java:24-29
void ElectrocuteAI::handleSpawned() {
	NpcAI::handleSpawned();
	task.set(ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] { AIActions::useSkill(*this, 20757); }, 0, 2000));
	despawn();
}

// Java ElectrocuteAI.java:31-33
void ElectrocuteAI::despawn() {
	ThreadPoolManager::getInstance().schedule({this}, [this] { getOwner().getController().delete_(); }, 10500);
}

// Java ElectrocuteAI.java:35-39
void ElectrocuteAI::handleDespawned() {
	task.get()->cancel(true);
	NpcAI::handleDespawned();
}

// Java ElectrocuteAI.java:41-47
bool ElectrocuteAI::ask(AIQuestion question) {
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
