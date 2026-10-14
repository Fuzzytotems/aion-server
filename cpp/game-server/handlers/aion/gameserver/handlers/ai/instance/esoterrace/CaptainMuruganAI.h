#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::esoterrace {

/**
 * The AI of Captain Murugan in Esoterrace ("captain_murugan"): the first attack opens with 19324 on a player target; every 20 seconds he
 * shouts and casts 19325 on himself, and below half HP 19324 twice more (10 and 14 seconds later); the tasks end with his death, despawn or
 * return home.
 * <p>
 * Java: data/handlers/ai/instance/esoterrace/CaptainMuruganAI.java, @AIName("captain_murugan"). C++: the anonymous Runnables are lambdas
 * pinned to this AI.
 *
 * @author xTz
 */
class CaptainMuruganAI : public AggressiveNpcAI {
private:
	runtime::AtomicBoolean isAggred{AION_LOCK_CLASS(CaptainMuruganAI::isAggred)}; // Java: = new AtomicBoolean(false)
	runtime::Field<runtime::FutureRef> task{};
	runtime::Field<runtime::FutureRef> specialSkillTask{};

public:
	explicit CaptainMuruganAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;

private:
	void startTaskEvent();

	/** the body of Java's first inner Runnable (the 20 s task) */
	void runTask();

	void castOnPlayerTarget();

	void cancelTask();

	void cancelSpecialSkillTask();

protected:
	void handleBackHome() override;

	void handleDespawned() override;

	void handleDied() override;
};

} // namespace aion::gameserver::handlers::ai::instance::esoterrace
