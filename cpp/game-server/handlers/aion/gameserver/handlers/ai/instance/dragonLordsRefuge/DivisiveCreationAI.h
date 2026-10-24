#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge {

/**
 * The AI of the divisive creations ("divisive_creation"): earth; 5 s after the spawn it walks to a random player inside; 20986 deals 60 %,
 * 21897 and 21898 half; no decay, respawn or rewards.
 * <p>
 * Java: data/handlers/ai/instance/dragonLordsRefuge/DivisiveCreationAI.java, @AIName("divisive_creation") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class DivisiveCreationAI : public AggressiveNpcAI {
public:
	explicit DivisiveCreationAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleSpawned() override;

public:
	float modifyOwnerDamage(float damage, Creature& effected, runtime::Ptr<Effect> effect) override;

	ItemAttackType modifyAttackType(ItemAttackType type) override;

	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge
