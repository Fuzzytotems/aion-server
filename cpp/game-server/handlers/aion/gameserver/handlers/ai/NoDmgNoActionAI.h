#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

namespace aion::gameserver::handlers::ai {

/**
 * The AI of an npc that takes no damage and does nothing ("no_dmg_no_action": the conquest offering spawn points).
 * <p>
 * Java: data/handlers/ai/NoDmgNoActionAI.java, @AIName("no_dmg_no_action").
 */
class NoDmgNoActionAI : public NpcAI {
public:
	explicit NoDmgNoActionAI(Npc& owner) : NpcAI(owner) {}

	float modifyDamage(Creature& attacker, float damage, runtime::Ptr<Effect> effect) override;
};

} // namespace aion::gameserver::handlers::ai
