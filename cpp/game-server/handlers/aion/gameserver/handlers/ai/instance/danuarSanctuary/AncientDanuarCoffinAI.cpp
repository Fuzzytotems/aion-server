#include "aion/gameserver/handlers/ai/instance/danuarSanctuary/AncientDanuarCoffinAI.h"

#include "aion/commons/utils/Rnd.h"

namespace aion::gameserver::handlers::ai::instance::danuarSanctuary {

AION_AI(AncientDanuarCoffinAI, "ancientdanuarcoffin");

// Java AncientDanuarCoffinAI.java:21-24
bool AncientDanuarCoffinAI::canThink() {
	return false;
}

// Java AncientDanuarCoffinAI.java:26-29
float AncientDanuarCoffinAI::modifyDamage(Creature& attacker, float damage, runtime::Ptr<Effect> effect) {
	return 1;
}

// Java AncientDanuarCoffinAI.java:31-37
void AncientDanuarCoffinAI::handleDied() {
	GeneralNpcAI::handleDied();
	if (commons::utils::Rnd::chance() < 40) {
		spawn(233085, getOwner().getX(), getOwner().getY(), getOwner().getZ(), int8_t{0});
	}
}

} // namespace aion::gameserver::handlers::ai::instance::danuarSanctuary
