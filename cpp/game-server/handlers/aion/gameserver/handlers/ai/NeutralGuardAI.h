#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

namespace aion::gameserver::handlers::ai {

/**
 * The AI of a neutral guard ("neutralguard"): supportive until a creature near it attacks a player, then aggressive towards it.
 * <p>
 * Java: data/handlers/ai/NeutralGuardAI.java, @AIName("neutralguard").
 */
class NeutralGuardAI : public AggressiveNpcAI {
public:
	explicit NeutralGuardAI(Npc& owner) : AggressiveNpcAI(owner) {}

	void creatureNeedsHelp(Creature& attacker) override;

protected:
	void handleBackHome() override;
};

} // namespace aion::gameserver::handlers::ai
