#include "aion/gameserver/handlers/ai/worlds/panesterra/ahserionsflight/AhserionConstructAI.h"

#include "aion/gameserver/configs/main/SiegeConfig.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::handlers::ai::worlds::panesterra::ahserionsflight {

// Java AhserionConstructAI.java:22-26
void AhserionConstructAI::modifyOwnerStat(Stat2& stat) {
	if (stat.getStat() == StatEnum::MAXHP)
		stat.setBaseRate(SiegeConfig::AHSERION_MAX_PLAYERS_PER_TEAM.load() / 100.0f);
}

// Java AhserionConstructAI.java:28-40
float AhserionConstructAI::modifyDamage(Creature& attacker, float damage, runtime::Ptr<Effect> effect) {
	if (dynamic_cast<Npc*>(&attacker) != nullptr && effect != nullptr) {
		switch (effect->getSkillId()) {
			case 21755: // Bombarding targets.
			case 21578: // Shield Penetration
			case 21583: // Artillery Blast
			case 21584: // Area Bombardment
				return damage * (SiegeConfig::AHSERION_MAX_PLAYERS_PER_TEAM.load() / 100.0f);
			default:
				break;
		}
	}
	return NpcAI::modifyDamage(attacker, damage, effect);
}

// Java AhserionConstructAI.java:42-48
bool AhserionConstructAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::ALLOW_RESPAWN:
		case AIQuestion::REWARD_LOOT:
			return false;
		default:
			return NpcAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai::worlds::panesterra::ahserionsflight
