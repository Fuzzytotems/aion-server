#include "aion/gameserver/handlers/ai/instance/ophidianBridge/DefenseWallAI.h"

namespace aion::gameserver::handlers::ai::instance::ophidianBridge {

AION_AI(DefenseWallAI, "defense_wall");

// Java DefenseWallAI.java:19-22
bool DefenseWallAI::canThink() {
	return false;
}

// Java DefenseWallAI.java:24-30
bool DefenseWallAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::ALLOW_DECAY:
		case AIQuestion::REWARD_AP_XP_DP_LOOT:
			return false;
		default:
			return GeneralNpcAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::ophidianBridge
