#include "aion/gameserver/handlers/ai/instance/rentusBase/DrakanBarricadeAI.h"


namespace aion::gameserver::handlers::ai::instance::rentusBase {

AION_AI(DrakanBarricadeAI, "drakanbarricade");

// Java DrakanBarricadeAI.java:19-24
bool DrakanBarricadeAI::ask(AIQuestion question) {
	if (question == AIQuestion::REWARD_LOOT)
		return false;
	return NoActionAI::ask(question);
}

} // namespace aion::gameserver::handlers::ai::instance::rentusBase
