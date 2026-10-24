#include "aion/gameserver/handlers/ai/worlds/inggison/SematariuxThunderShieldAI.h"

#include "aion/gameserver/model/stats/calc/Stat2.h"

namespace aion::gameserver::handlers::ai::worlds::inggison {

AION_AI(SematariuxThunderShieldAI, "sematariux_thunder_shield");

// Java SematariuxThunderShieldAI.java:22-26
void SematariuxThunderShieldAI::modifyOwnerStat(Stat2& stat) {
	if (stat.getStat() == StatEnum::MAXHP)
		stat.setBaseRate(0.1f);
}

// Java SematariuxThunderShieldAI.java:28-31
float SematariuxThunderShieldAI::modifyOwnerDamage(float damage, Creature& /*effected*/, runtime::Ptr<Effect> /*effect*/) {
	return damage * 0.5f;
}

} // namespace aion::gameserver::handlers::ai::worlds::inggison
