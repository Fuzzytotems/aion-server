#include "aion/gameserver/handlers/ai/siege/ShieldNpcAI.h"

#include "aion/gameserver/model/siege/FortressLocation.h"
#include "aion/gameserver/model/templates/spawns/siegespawns/SiegeSpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SHIELD_EFFECT.h"
#include "aion/gameserver/services/SiegeService.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::siege {

AION_AI(ShieldNpcAI, "siege_shieldnpc");

// Java ShieldNpcAI.java:21-25
bool ShieldNpcAI::canThink() {
	// prevent field stone from resetting
	return getOwner().getRace() != Race::CONSTRUCT;
}

// Java ShieldNpcAI.java:27-31
void ShieldNpcAI::handleDespawned() {
	updateFortressShieldStatus(false);
	SiegeNpcAI::handleDespawned();
}

// Java ShieldNpcAI.java:33-37
void ShieldNpcAI::handleSpawned() {
	updateFortressShieldStatus(true);
	SiegeNpcAI::handleSpawned();
}

// Java ShieldNpcAI.java:39-46
bool ShieldNpcAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::REWARD_AP_XP_DP_LOOT:
		case AIQuestion::REWARD_AP:
			return true;
		case AIQuestion::REWARD_LOOT:
		case AIQuestion::ALLOW_RESPAWN:
			return false;
		default:
			return SiegeNpcAI::ask(question);
	}
}

// Java ShieldNpcAI.java:48-52
void ShieldNpcAI::updateFortressShieldStatus(bool hasShield) {
	const int32_t siegeLocationId = getSpawnTemplate()->getSiegeId();
	SiegeService::getInstance().getFortress(siegeLocationId)->setUnderShield(hasShield);
	PacketSendUtility::broadcastToMap(*getPosition()->getWorldMapInstance(), SM_SHIELD_EFFECT(siegeLocationId));
}

} // namespace aion::gameserver::handlers::ai::siege
