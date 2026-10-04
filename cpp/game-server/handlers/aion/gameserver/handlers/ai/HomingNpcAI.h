#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"

namespace aion::gameserver::handlers::ai {

/**
 * The AI of a homing ("homing": the Spiritmaster's Cyclone Servant and the like): it attacks the target it was sent at, with a skill when one is
 * ready, and never thinks of going home.
 * <p>
 * Java: data/handlers/ai/HomingNpcAI.java, @AIName("homing") (the marker is in the .cpp).
 */
class HomingNpcAI : public GeneralNpcAI {
public:
	explicit HomingNpcAI(Npc& owner) : GeneralNpcAI(owner) {}

	void think() override;

	AttackIntention chooseAttackIntention() override;

	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai
