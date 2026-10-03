#include "aion/gameserver/handlers/ai/quests/AscensationNpcAI.h"

namespace aion::gameserver::handlers::ai::quests {

AION_AI(AscensationNpcAI, "ascensationquestnpc");

// Java AscensationNpcAI.java:20-23
float AscensationNpcAI::modifyOwnerDamage(float damage, Creature& effected, runtime::Ptr<Effect> effect) {
	static_cast<void>(damage);
	static_cast<void>(effected);
	static_cast<void>(effect);
	return 1;
}

} // namespace aion::gameserver::handlers::ai::quests
