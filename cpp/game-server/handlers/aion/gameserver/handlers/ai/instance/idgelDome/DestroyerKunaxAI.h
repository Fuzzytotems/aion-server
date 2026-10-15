#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::idgelDome {

/**
 * The AI of Destroyer Kunax in Idgel Dome ("destroyer_kunax"): a target between 3 and 30 m beyond the bound radii gets skill 21550.
 * <p>
 * Java: data/handlers/ai/instance/idgelDome/DestroyerKunaxAI.java, @AIName("destroyer_kunax") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class DestroyerKunaxAI : public AggressiveNpcAI {
public:
	explicit DestroyerKunaxAI(Npc& owner) : AggressiveNpcAI(owner) {}

	AttackIntention chooseAttackIntention() override;
};

} // namespace aion::gameserver::handlers::ai::instance::idgelDome
