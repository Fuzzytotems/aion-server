#include "aion/gameserver/handlers/ai/instance/tiamatStrongHold/BladeStormAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/controllers/NpcController.h"

namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold {

AION_AI(BladeStormAI, "bladestorm");

// Java BladeStormAI.java:24-29
void BladeStormAI::handleSpawned() {
	NpcAI::handleSpawned();
	spinTask.set(ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] { AIActions::useSkill(*this, 20748); }, 0, 1000));
	ThreadPoolManager::getInstance().schedule({this}, [this] { getOwner().getController().delete_(); }, 10000);
}

// Java BladeStormAI.java:31-35
void BladeStormAI::handleDespawned() {
	spinTask.get()->cancel(true);
	NpcAI::handleDespawned();
}

// Java BladeStormAI.java:37-43
bool BladeStormAI::ask(AIQuestion question) {
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
