#include "aion/gameserver/handlers/ai/worlds/heiron/KlawspawnAI.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::worlds::heiron {

AION_AI(KlawspawnAI, "klawspawn");

// Java KlawspawnAI.java:22-25
bool KlawspawnAI::canThink() {
	return false;
}

// Java KlawspawnAI.java:27-37
void KlawspawnAI::handleAttack(runtime::Ptr<Creature> creature) {
	GeneralNpcAI::handleAttack(creature);
	runtime::Ptr<Npc> npc = getOwner().getPosition()->getWorldMapInstance()->getNpc(212120);
	if (npc == nullptr) {
		if (commons::utils::Rnd::chance() < 10) {
			spawn(212120, getOwner().getX(), getOwner().getY(), getOwner().getZ(), int8_t{0});
			AIActions::die(*this, *creature);
		}
	}
}

// Java KlawspawnAI.java:39-42
float KlawspawnAI::modifyDamage(Creature& /*attacker*/, float /*damage*/, runtime::Ptr<Effect> /*effect*/) {
	return 1;
}

// Java KlawspawnAI.java:44-48
void KlawspawnAI::handleDied() {
	GeneralNpcAI::handleDied();
	AIActions::deleteOwner(*this);
}

} // namespace aion::gameserver::handlers::ai::worlds::heiron
