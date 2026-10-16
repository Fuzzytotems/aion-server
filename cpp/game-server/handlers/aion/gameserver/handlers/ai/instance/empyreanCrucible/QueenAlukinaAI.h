#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/ai/HpPhases.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::empyreanCrucible {

/**
 * Queen Alukina of Empyrean Crucible ("alukina_emp"): a skill at each of 75, 50 and 25 %, timed follow-ups, and from 25 % a skill round
 * every 20 s.
 * <p>
 * Java: data/handlers/ai/instance/empyreanCrucible/QueenAlukinaAI.java, @AIName("alukina_emp") (the marker is in the .cpp).
 *
 * @author Luzien
 */
class QueenAlukinaAI : public AggressiveNpcAI, public gameserver::ai::HpPhases::PhaseHandler {
private:
	const runtime::Ref<gameserver::ai::HpPhases> hpPhases = gameserver::ai::HpPhases::create(75, {50, 25}); // Java: = new HpPhases(75, 50, 25)
	runtime::Field<runtime::FutureRef> task{};

public:
	explicit QueenAlukinaAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleDespawned() override;

	void handleDied() override;

	void handleBackHome() override;

	void handleAttack(runtime::Ptr<Creature> creature) override;

public:
	void handleHpPhase(int32_t phaseHpPercent) override;

private:
	void cancelTask();

	void scheduleSkill(int32_t skill, int32_t delay);
};

} // namespace aion::gameserver::handlers::ai::instance::empyreanCrucible
