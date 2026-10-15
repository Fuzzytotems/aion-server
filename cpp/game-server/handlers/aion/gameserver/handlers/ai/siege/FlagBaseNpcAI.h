#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

namespace aion::gameserver::handlers::ai::siege {

/**
 * The flag of a siege base ("base_flag"): it neither takes nor deals damage.
 * <p>
 * Java: data/handlers/ai/siege/FlagBaseNpcAI.java, @AIName("base_flag") (the marker is in the .cpp). M5i I-8, under P5-12b1's lease.
 *
 * @author Bobobear
 */
class FlagBaseNpcAI : public NpcAI {
public:
	explicit FlagBaseNpcAI(Npc& owner) : NpcAI(owner) {}

	float modifyDamage(Creature& attacker, float damage, runtime::Ptr<Effect> effect) override;

	float modifyOwnerDamage(float damage, Creature& effected, runtime::Ptr<Effect> effect) override;
};

} // namespace aion::gameserver::handlers::ai::siege
