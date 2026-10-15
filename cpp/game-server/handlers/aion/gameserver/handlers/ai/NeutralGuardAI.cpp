#include "aion/gameserver/handlers/ai/NeutralGuardAI.h"

#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/utils/PositionUtil.h"

namespace aion::gameserver::handlers::ai {

AION_AI(NeutralGuardAI, "neutralguard");

// Java NeutralGuardAI.java:22-26
void NeutralGuardAI::handleBackHome() {
	AggressiveNpcAI::handleBackHome();
	getOwner().overrideNpcType(CreatureType::SUPPORT);
}

// Java NeutralGuardAI.java:28-35
void NeutralGuardAI::creatureNeedsHelp(Creature& attacker) {
	if (PositionUtil::isInRange(attacker, getOwner(), 20) && getOwner().getType(attacker) != CreatureType::AGGRESSIVE
		&& runtime::as<Player>(attacker.getTarget()) != nullptr) {
		getOwner().overrideNpcType(CreatureType::AGGRESSIVE);
		getOwner().getAggroList().addHate(attacker, 1000);
	}
}

} // namespace aion::gameserver::handlers::ai
