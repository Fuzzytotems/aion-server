#include "aion/gameserver/handlers/ai/instance/custom/eternalChallenge/CustomInstanceScapiAI.h"

namespace aion::gameserver::handlers::ai::instance::custom::eternalChallenge {

AION_AI(CustomInstanceScapiAI, "custom_instance_scapi");

// Java CustomInstanceScapiAI.java:19-22
float CustomInstanceScapiAI::modifyDamage(Creature& attacker, float damage, runtime::Ptr<Effect> effect) {
	static_cast<void>(attacker);
	static_cast<void>(damage);
	static_cast<void>(effect);
	return 1;
}

} // namespace aion::gameserver::handlers::ai::instance::custom::eternalChallenge
