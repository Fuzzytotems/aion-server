#pragma once

#include "aion/gameserver/handlers/ai/AggressiveNoLootNpcAI.h"
#include "aion/gameserver/handlers/ai/AiPrelude.h"

namespace aion::gameserver::handlers::ai::worlds::panesterra::ahserionsflight {

/**
 * A camp's defense cannon in Ahserion's Flight ("camp_defense_cannon"): the construct scaling of max hp and the four siege skills' damage, magical
 * fire attacks; after an attack it stops attacking and loses its aggro without restoring hp.
 * <p>
 * Java: data/handlers/ai/worlds/panesterra/ahserionsflight/CampDefenseCannonAI.java, @AIName("camp_defense_cannon") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class CampDefenseCannonAI : public AggressiveNoLootNpcAI {
public:
	explicit CampDefenseCannonAI(Npc& owner) : AggressiveNoLootNpcAI(owner) {}

	void modifyOwnerStat(Stat2& stat) override;

	float modifyDamage(Creature& attacker, float damage, runtime::Ptr<Effect> effect) override;

	ItemAttackType modifyAttackType(ItemAttackType type) override;

	void handleFinishAttack() override;
};

} // namespace aion::gameserver::handlers::ai::worlds::panesterra::ahserionsflight
