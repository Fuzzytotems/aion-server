#include "aion/gameserver/handlers/ai/instance/tiamatStrongHold/TahabataAltarFinalAI.h"

#include "aion/gameserver/ai/AIActions.h"

namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold {

AION_AI(TahabataAltarFinalAI, "tahabataaltar2");

// Java TahabataAltarFinalAI.java:24-28
void TahabataAltarFinalAI::handleSpawned() {
	NpcAI::handleSpawned();
	task.set(ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] { AIActions::useSkill(*this, 20972); }, 0, 2000));
}

// Java TahabataAltarFinalAI.java:30-34
void TahabataAltarFinalAI::handleDespawned() {
	task.get()->cancel(true);
	NpcAI::handleDespawned();
}

// Java TahabataAltarFinalAI.java:36-42
bool TahabataAltarFinalAI::ask(AIQuestion question) {
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
