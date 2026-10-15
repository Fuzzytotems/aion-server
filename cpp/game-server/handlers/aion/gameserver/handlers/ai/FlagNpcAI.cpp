#include "aion/gameserver/handlers/ai/FlagNpcAI.h"

namespace aion::gameserver::handlers::ai {

AION_AI(FlagNpcAI, "flag");

// Java FlagNpcAI.java:19-22
float FlagNpcAI::modifyDamage(Creature& /*attacker*/, float /*damage*/, runtime::Ptr<Effect> /*effect*/) {
	return 0;
}

} // namespace aion::gameserver::handlers::ai
