#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

namespace aion::gameserver::handlers::ai {

/**
 * The AI of an aggressive npc that takes and deals 1 damage per hit and neither evades nor resists ("onedmg_aggressive").
 * <p>
 * Java: data/handlers/ai/OneDmgAI.java, @AIName("onedmg_aggressive").
 */
class OneDmgAI : public AggressiveNpcAI {
public:
	explicit OneDmgAI(Npc& owner) : AggressiveNpcAI(owner) {}

	float modifyDamage(Creature& attacker, float damage, runtime::Ptr<Effect> effect) override;

	float modifyOwnerDamage(float damage, Creature& effected, runtime::Ptr<Effect> effect) override;

	void modifyOwnerStat(Stat2& stat) override;
};

} // namespace aion::gameserver::handlers::ai
