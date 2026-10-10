#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

namespace aion::gameserver::handlers::ai {

/**
 * The AI of a passive npc that takes 1 damage per hit and neither evades nor resists ("onedmg_passive": training dummies of the
 * housing maps).
 * <p>
 * Java: data/handlers/ai/OneDmgNoActionAI.java, @AIName("onedmg_passive").
 */
class OneDmgNoActionAI : public NpcAI {
public:
	explicit OneDmgNoActionAI(Npc& owner) : NpcAI(owner) {}

	float modifyDamage(Creature& attacker, float damage, runtime::Ptr<Effect> effect) override;

	void modifyOwnerStat(Stat2& stat) override;
};

} // namespace aion::gameserver::handlers::ai
