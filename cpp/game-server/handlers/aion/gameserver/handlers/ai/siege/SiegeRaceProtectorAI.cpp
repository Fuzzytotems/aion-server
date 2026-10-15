#include "aion/gameserver/handlers/ai/siege/SiegeRaceProtectorAI.h"

namespace aion::gameserver::handlers::ai::siege {

AION_AI(SiegeRaceProtectorAI, "siege_raceprotector");

// Java SiegeRaceProtectorAI.java:17-23
bool SiegeRaceProtectorAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::ALLOW_DECAY:
		case AIQuestion::REWARD_AP_XP_DP_LOOT:
		case AIQuestion::REWARD_LOOT:
			return true;
		default:
			return SiegeNpcAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai::siege
