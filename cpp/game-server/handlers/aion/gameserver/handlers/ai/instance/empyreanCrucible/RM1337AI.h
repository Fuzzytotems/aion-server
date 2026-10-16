#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::empyreanCrucible {

/**
 * RM-1337 of Empyrean Crucible ("rm_1337"): a skill every 23 s once attacked (two kinds below 50 %), and from 75 % a minute's round of
 * its big skill with 8 to 12 sparks (282373).
 * <p>
 * Java: data/handlers/ai/instance/empyreanCrucible/RM1337AI.java, @AIName("rm_1337") (the marker is in the .cpp).
 *
 * @author Luzien
 */
class RM1337AI : public AggressiveNpcAI {
private:
	runtime::AtomicBoolean isHome{AION_LOCK_CLASS(RM1337AI::isHome), true}; // Java: = new AtomicBoolean(true)
	runtime::AtomicBoolean isEventStarted{AION_LOCK_CLASS(RM1337AI::isEventStarted), false}; // Java: = new AtomicBoolean(false)
	runtime::Field<runtime::FutureRef> task1{}, task2{};

public:
	explicit RM1337AI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleSpawned() override;

	void handleDespawned() override;

	void handleDied() override;

	void handleBackHome() override;

	void handleAttack(runtime::Ptr<Creature> creature) override;

private:
	void cancelTask();

	void startSkillTask1();

	void startSkillTask2();

	void spawnSparks();

	runtime::Ptr<Creature> getRandomTarget();
};

} // namespace aion::gameserver::handlers::ai::instance::empyreanCrucible
