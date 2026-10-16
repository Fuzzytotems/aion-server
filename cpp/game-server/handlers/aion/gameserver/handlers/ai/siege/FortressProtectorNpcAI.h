#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/siege/AbstractSiegeProtectorAI.h"

namespace aion::gameserver::handlers::ai::siege {

/**
 * The protector of a fortress, its guardian deity general ("fortress_protector"): five times the damage against npcs, and the maximum hp of a
 * legendary one takes the fortress protector multiplier of SiegeConfig.
 * <p>
 * Java: data/handlers/ai/siege/FortressProtectorNpcAI.java, @AIName("fortress_protector") (the marker is in the .cpp). M5i I-2, an A1 lease.
 *
 * @author ATracer, Source
 */
class FortressProtectorNpcAI : public AbstractSiegeProtectorAI {
public:
	explicit FortressProtectorNpcAI(Npc& owner) : AbstractSiegeProtectorAI(owner) {}

	float modifyOwnerDamage(float damage, Creature& effected, runtime::Ptr<Effect> effect) override;

	void modifyOwnerStat(Stat2& stat) override;
};

} // namespace aion::gameserver::handlers::ai::siege
