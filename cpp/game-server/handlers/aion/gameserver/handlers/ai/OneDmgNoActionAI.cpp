#include "aion/gameserver/handlers/ai/OneDmgNoActionAI.h"

#include "aion/gameserver/model/stats/calc/Stat2.h"

namespace aion::gameserver::handlers::ai {

AION_AI(OneDmgNoActionAI, "onedmg_passive");

// Java OneDmgNoActionAI.java:21-24
float OneDmgNoActionAI::modifyDamage(Creature& /*attacker*/, float /*damage*/, runtime::Ptr<Effect> /*effect*/) {
	return 1;
}

// Java OneDmgNoActionAI.java:26-33
void OneDmgNoActionAI::modifyOwnerStat(Stat2& stat) {
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
