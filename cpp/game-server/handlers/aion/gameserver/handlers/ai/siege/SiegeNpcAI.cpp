#include "aion/gameserver/handlers/ai/siege/SiegeNpcAI.h"

#include "aion/gameserver/model/templates/spawns/siegespawns/SiegeSpawnTemplate.h"
#include "aion/gameserver/services/SiegeService.h"
#include "aion/gameserver/services/siege/Siege.h"

namespace aion::gameserver::handlers::ai::siege {

// Java SiegeNpcAI.java:21-27
bool SiegeNpcAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::ALLOW_DECAY:
		case AIQuestion::ALLOW_RESPAWN:
		case AIQuestion::REWARD_LOOT:
		case AIQuestion::REMOVE_EFFECTS_ON_MAP_REGION_DEACTIVATE:
			return false;
		default:
			return AggressiveNpcAI::ask(question);
	}
}

// Java SiegeNpcAI.java:29-31
runtime::Ptr<Siege> SiegeNpcAI::getSiege() {
	return SiegeService::getInstance().getSiege(getSpawnTemplate()->getSiegeId());
}

// Java SiegeNpcAI.java:33-36
runtime::Ptr<SiegeSpawnTemplate> SiegeNpcAI::getSpawnTemplate() {
	return runtime::cast<SiegeSpawnTemplate>(AggressiveNpcAI::getSpawnTemplate());
}

} // namespace aion::gameserver::handlers::ai::siege
