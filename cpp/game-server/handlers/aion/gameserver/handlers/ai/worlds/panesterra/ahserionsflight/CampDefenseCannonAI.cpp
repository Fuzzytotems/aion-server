#include "aion/gameserver/handlers/ai/worlds/panesterra/ahserionsflight/CampDefenseCannonAI.h"

#include "aion/gameserver/ai/manager/EmoteManager.h"
#include "aion/gameserver/configs/main/SiegeConfig.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::handlers::ai::worlds::panesterra::ahserionsflight {

AION_AI(CampDefenseCannonAI, "camp_defense_cannon");

// Java CampDefenseCannonAI.java:24-28
void CampDefenseCannonAI::modifyOwnerStat(Stat2& stat) {
	if (stat.getStat() == StatEnum::MAXHP)
		stat.setBaseRate(SiegeConfig::AHSERION_MAX_PLAYERS_PER_TEAM.load() / 100.0f);
}

// Java CampDefenseCannonAI.java:30-42
float CampDefenseCannonAI::modifyDamage(Creature& attacker, float damage, runtime::Ptr<Effect> effect) {
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
	return AggressiveNoLootNpcAI::modifyDamage(attacker, damage, effect);
}

// Java CampDefenseCannonAI.java:44-47
ItemAttackType CampDefenseCannonAI::modifyAttackType(ItemAttackType type) {
	static_cast<void>(type);
	return ItemAttackType::MAGICAL_FIRE;
}

// Java CampDefenseCannonAI.java:49-57 (no super call, as Java)
void CampDefenseCannonAI::handleFinishAttack() {
	if (!canThink())
		return;
	Npc& npc = getOwner();
	EmoteManager::emoteStopAttacking(npc);
	npc.getController().loseAggro(false);
	npc.setSkillNumber(0);
}

} // namespace aion::gameserver::handlers::ai::worlds::panesterra::ahserionsflight
