#include "aion/gameserver/handlers/ai/instance/dragonLordsRefuge/TiamatsIncarnationSpawnsAI.h"

#include "aion/gameserver/ai/AIActions.h"

namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge {

AION_AI(TiamatsIncarnationSpawnsAI, "tiamats_incarnation_spawn");

// Java TiamatsIncarnationSpawnsAI.java:24-28
void TiamatsIncarnationSpawnsAI::handleSpawned() {
	NpcAI::handleSpawned();
	skillTask.set(ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] { AIActions::useSkill(*this, getSkillId()); }, 2500, 3000));
}

// Java TiamatsIncarnationSpawnsAI.java:30-34
void TiamatsIncarnationSpawnsAI::handleDespawned() {
	skillTask.get()->cancel(true);
	NpcAI::handleDespawned();
}

// Java TiamatsIncarnationSpawnsAI.java:36-56
int32_t TiamatsIncarnationSpawnsAI::getSkillId() {
	switch (getNpcId()) {
		case 282727: // Gravity Whirlpool
		case 856074:
			return 20155;
		case 282729: // Thunderbolt Whirlpool
		case 856076:
			return 20156;
		case 282731: // Petrification Crystal
		case 856072:
			return 20159;
		case 282735: // Cavity of Earth
		case 856068:
			return 20172;
		case 282737: // Collapsing Earth
		case 856070:
			return 20173;
		default:
			return 0;
	}
}

// Java TiamatsIncarnationSpawnsAI.java:58-64
bool TiamatsIncarnationSpawnsAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::REWARD_AP_XP_DP_LOOT:
		case AIQuestion::ALLOW_DECAY:
			return false;
		default:
			return NpcAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge
