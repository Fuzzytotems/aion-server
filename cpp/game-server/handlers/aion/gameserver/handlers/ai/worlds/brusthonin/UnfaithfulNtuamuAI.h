#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/ai/HpPhases.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::handlers::ai::worlds::brusthonin {

/**
 * The AI of Unfaithful Ntuamu in Brusthonin ("unfaithfulntuamu"): at 50 % HP she turns into the Vampire Queen (214583), who appears where
 * she stood with half her HP, and is deleted; her respawn comes with the queen's death.
 * <p>
 * Java: data/handlers/ai/worlds/brusthonin/UnfaithfulNtuamuAI.java, @AIName("unfaithfulntuamu") (the marker is in the .cpp).
 *
 * @author Cheatkiller, Neon
 */
class UnfaithfulNtuamuAI : public AggressiveNpcAI, public gameserver::ai::HpPhases::PhaseHandler {
private:
	const runtime::Ref<gameserver::ai::HpPhases> hpPhases = gameserver::ai::HpPhases::create(50); // Java: = new HpPhases(50)

public:
	explicit UnfaithfulNtuamuAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;

public:
	void handleHpPhase(int32_t phaseHpPercent) override;
};

} // namespace aion::gameserver::handlers::ai::worlds::brusthonin
