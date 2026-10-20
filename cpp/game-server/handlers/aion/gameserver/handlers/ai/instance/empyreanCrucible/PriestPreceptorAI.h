#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/ai/HpPhases.h"

namespace aion::gameserver::handlers::ai::instance::empyreanCrucible {

/**
 * The priest preceptor of Empyrean Crucible's stage 8 ("priest_preceptor"): a buff after her spawn, a skill at 75 %, three soul-sick adds
 * (282366 to 282368) at 25 %.
 * <p>
 * Java: data/handlers/ai/instance/empyreanCrucible/PriestPreceptorAI.java, @AIName("priest_preceptor") (the marker is in the .cpp).
 *
 * @author Luzien
 */
class PriestPreceptorAI : public AggressiveNpcAI, public gameserver::ai::HpPhases::PhaseHandler {
private:
	const runtime::Ref<gameserver::ai::HpPhases> hpPhases = gameserver::ai::HpPhases::create(75, {25}); // Java: = new HpPhases(75, 25)

public:
	explicit PriestPreceptorAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleSpawned() override;

	void handleBackHome() override;

	void handleAttack(runtime::Ptr<Creature> creature) override;

public:
	void handleHpPhase(int32_t phaseHpPercent) override;

private:
	void startEvent();

	runtime::Ptr<Creature> getRandomTarget();

	void applySoulSickness(Npc& npc);
};

} // namespace aion::gameserver::handlers::ai::instance::empyreanCrucible
