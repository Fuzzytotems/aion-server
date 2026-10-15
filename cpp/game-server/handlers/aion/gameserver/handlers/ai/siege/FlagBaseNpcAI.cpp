#include "aion/gameserver/handlers/ai/siege/FlagBaseNpcAI.h"

namespace aion::gameserver::handlers::ai::siege {

AION_AI(FlagBaseNpcAI, "base_flag");

// Java FlagBaseNpcAI.java:20-23
float FlagBaseNpcAI::modifyDamage(Creature& attacker, float damage, runtime::Ptr<Effect> effect) {
	static_cast<void>(attacker);
	static_cast<void>(damage);
	static_cast<void>(effect);
	return 0;
}

// Java FlagBaseNpcAI.java:25-28
float FlagBaseNpcAI::modifyOwnerDamage(float damage, Creature& effected, runtime::Ptr<Effect> effect) {
	static_cast<void>(damage);
	static_cast<void>(effected);
	static_cast<void>(effect);
	return 0;
}

} // namespace aion::gameserver::handlers::ai::siege
