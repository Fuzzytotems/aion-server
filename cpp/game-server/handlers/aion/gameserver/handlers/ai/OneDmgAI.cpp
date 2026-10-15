#include "aion/gameserver/handlers/ai/OneDmgAI.h"

#include "aion/gameserver/model/stats/calc/Stat2.h"

namespace aion::gameserver::handlers::ai {

AION_AI(OneDmgAI, "onedmg_aggressive");

// Java OneDmgAI.java:21-24
float OneDmgAI::modifyDamage(Creature& /*attacker*/, float /*damage*/, runtime::Ptr<Effect> /*effect*/) {
	return 1;
}

// Java OneDmgAI.java:26-29
float OneDmgAI::modifyOwnerDamage(float /*damage*/, Creature& /*effected*/, runtime::Ptr<Effect> /*effect*/) {
	return 1;
}

// Java OneDmgAI.java:31-38
void OneDmgAI::modifyOwnerStat(Stat2& stat) {
	switch (stat.getStat()) { // ai owner should not evade or resist
		case StatEnum::MAGICAL_RESIST:
		case StatEnum::EVASION:
			stat.setBase(0);
			stat.setBonus(0);
			break;
		default:
			break;
	}
}

} // namespace aion::gameserver::handlers::ai
