#include "aion/gameserver/handlers/ai/worlds/inggison/OmegaAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::handlers::ai::worlds::inggison {

AION_AI(OmegaAI, "omega");

// Java OmegaAI.java:20-24
void OmegaAI::handleBeforeSpawn(const model::templates::ai::Percentage& /*percent*/) {
	AIActions::useSkill(*this, 19189);
	AIActions::useSkill(*this, 19191);
}

// Java OmegaAI.java:26-29
bool OmegaAI::checkBeforeSpawn() {
	for (const runtime::Ptr<Player>& player : getKnownList().streamPlayers()) { // Java: streamPlayers().anyMatch(player -> isInRange(player, 30))
		if (isInRange(*player, 30))
			return true;
	}
	return false;
}

} // namespace aion::gameserver::handlers::ai::worlds::inggison
