#include "aion/gameserver/handlers/ai/NoDmgNoActionAI.h"


namespace aion::gameserver::handlers::ai {

AION_AI(NoDmgNoActionAI, "no_dmg_no_action");

// Java NoDmgNoActionAI.java:20-23
float NoDmgNoActionAI::modifyDamage(Creature& /*attacker*/, float /*damage*/, runtime::Ptr<Effect> /*effect*/) {
	return 0;
}

} // namespace aion::gameserver::handlers::ai
