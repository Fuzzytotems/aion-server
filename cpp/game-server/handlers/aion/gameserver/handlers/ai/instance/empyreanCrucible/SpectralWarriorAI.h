#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/ai/HpPhases.h"

namespace aion::gameserver::handlers::ai::instance::empyreanCrucible {

/**
 * The spectral warrior of Empyrean Crucible's stage 6 ("spectral_warrior"): at 50 % it starts round 5 and, 2 s later, raises the fallen
 * (205413, 205414) it knows as 217576 and 217577.
 * <p>
 * Java: data/handlers/ai/instance/empyreanCrucible/SpectralWarriorAI.java, @AIName("spectral_warrior") (the marker is in the .cpp).
 *
 * @author Luzien
 */
class SpectralWarriorAI : public AggressiveNpcAI, public gameserver::ai::HpPhases::PhaseHandler {
private:
	const runtime::Ref<gameserver::ai::HpPhases> hpPhases = gameserver::ai::HpPhases::create(50); // Java: = new HpPhases(50)

public:
	explicit SpectralWarriorAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;

public:
	void handleHpPhase(int32_t phaseHpPercent) override;

private:
	void resurrectAllies();
};

} // namespace aion::gameserver::handlers::ai::instance::empyreanCrucible
