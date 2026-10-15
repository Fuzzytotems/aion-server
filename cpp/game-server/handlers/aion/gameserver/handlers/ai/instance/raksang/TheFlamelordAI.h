#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/ai/HpPhases.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::raksang {

/**
 * The AI of the Flamelord in Raksang Ruins ("the_flamelord"): the first attack is announced; at 90 % HP his 30 s flame round starts; at
 * 40, 30, 20 and 10 % he pauses it, and 5 s later sends one to four executors to their targets and resumes; his death opens door 118.
 * <p>
 * Java: data/handlers/ai/instance/raksang/TheFlamelordAI.java, @AIName("the_flamelord") (the marker is in the .cpp).
 *
 * @author xTz
 */
class TheFlamelordAI : public AggressiveNpcAI, public gameserver::ai::HpPhases::PhaseHandler {
private:
	const runtime::Ref<gameserver::ai::HpPhases> hpPhases = gameserver::ai::HpPhases::create(90, {40, 30, 20, 10});
	runtime::AtomicBoolean isAggred{AION_LOCK_CLASS(TheFlamelordAI::isAggred), false}; // Java: = new AtomicBoolean(false)
	runtime::Field<runtime::FutureRef> phaseTask{};

public:
	explicit TheFlamelordAI(Npc& owner) : AggressiveNpcAI(owner) {}

	void handleHpPhase(int32_t phaseHpPercent) override;

private:
	void startPhaseEvent(int32_t percent);

	void moveExecutor(int32_t executorId);

	void startPhaseTask();

	void cancelPhaseTask();

protected:
	void handleDied() override;

	void handleDespawned() override;

	void handleAttack(runtime::Ptr<Creature> creature) override;

	void handleBackHome() override;
};

} // namespace aion::gameserver::handlers::ai::instance::raksang
