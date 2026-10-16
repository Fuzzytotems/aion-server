#include "aion/gameserver/handlers/ai/worlds/inggison/HugeEggAI.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/ai/AIActions.h"

namespace aion::gameserver::handlers::ai::worlds::inggison {

AION_AI(HugeEggAI, "hugeegg");

// Java HugeEggAI.java:22-25
bool HugeEggAI::canThink() {
	return false;
}

// Java HugeEggAI.java:27-30
float HugeEggAI::modifyDamage(Creature& /*attacker*/, float /*damage*/, runtime::Ptr<Effect> /*effect*/) {
	return 1;
}

// Java HugeEggAI.java:32-39
void HugeEggAI::handleDied() {
	GeneralNpcAI::handleDied();
	if (commons::utils::Rnd::nextBoolean()) {
		spawn(217097, getOwner().getX(), getOwner().getY(), getOwner().getZ(), int8_t{0});
		AIActions::deleteOwner(*this);
	}
}

} // namespace aion::gameserver::handlers::ai::worlds::inggison
