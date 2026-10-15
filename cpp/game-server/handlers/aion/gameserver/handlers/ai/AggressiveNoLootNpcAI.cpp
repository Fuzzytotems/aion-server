#include "aion/gameserver/handlers/ai/AggressiveNoLootNpcAI.h"


namespace aion::gameserver::handlers::ai {

AION_AI(AggressiveNoLootNpcAI, "aggressive_no_loot");

// Java AggressiveNoLootNpcAI.java:20-25
bool AggressiveNoLootNpcAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::REWARD_LOOT:
		case AIQuestion::ALLOW_DECAY:
			return false;
		default:
			return AggressiveNpcAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai
