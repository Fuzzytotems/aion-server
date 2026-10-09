#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

namespace aion::gameserver::handlers::ai {

/**
 * The AI of an npc nobody interacts with ("no_interaction": no template uses it, //ai set reaches it): peaceful, no thinking but the
 * walker's, no damage.
 * <p>
 * Java: data/handlers/ai/NoInteractionAI.java, @AIName("no_interaction").
 */
class NoInteractionAI : public NpcAI {
public:
	explicit NoInteractionAI(Npc& owner) : NpcAI(owner) {}

	bool canThink() override;

	void think() override;

	float modifyDamage(Creature& attacker, float damage, runtime::Ptr<Effect> effect) override;

protected:
	void handleBeforeSpawned() override;

	void handleMoveArrived() override;
};

} // namespace aion::gameserver::handlers::ai
