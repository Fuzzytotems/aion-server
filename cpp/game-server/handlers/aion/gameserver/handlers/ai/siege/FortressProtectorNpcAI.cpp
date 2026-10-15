#include "aion/gameserver/handlers/ai/siege/FortressProtectorNpcAI.h"

#include "aion/gameserver/configs/main/SiegeConfig.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"

namespace aion::gameserver::handlers::ai::siege {

AION_AI(FortressProtectorNpcAI, "fortress_protector");

// Java FortressProtectorNpcAI.java:22-27
float FortressProtectorNpcAI::modifyOwnerDamage(float damage, Creature& effected, runtime::Ptr<Effect> effect) {
	if (dynamic_cast<Npc*>(&effected) != nullptr)
		return damage * 5;
	return damage;
}

// Java FortressProtectorNpcAI.java:29-33
void FortressProtectorNpcAI::modifyOwnerStat(Stat2& stat) {
	if (stat.getStat() == StatEnum::MAXHP && getOwner().getRating() == NpcRating::LEGENDARY)
		stat.setBaseRate(SiegeConfig::FORTRESS_PROTECTOR_HEALTH_MULTIPLIER.load());
}

} // namespace aion::gameserver::handlers::ai::siege
