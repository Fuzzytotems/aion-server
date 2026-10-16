#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::empyreanCrucible {

/**
 * The warrior preceptor of Empyrean Crucible's stage 8 ("warrior_preceptor"): every 30 s once attacked, Divine Grasp II and, 6 s later,
 * Wrathful Wave II.
 * <p>
 * Java: data/handlers/ai/instance/empyreanCrucible/WarriorPreceptorAI.java, @AIName("warrior_preceptor") (the marker is in the .cpp).
 *
 * @author Luzien
 */
class WarriorPreceptorAI : public AggressiveNpcAI {
private:
	runtime::AtomicBoolean isHome{AION_LOCK_CLASS(WarriorPreceptorAI::isHome), true}; // Java: = new AtomicBoolean(true)
	runtime::Field<runtime::FutureRef> task{};

public:
	explicit WarriorPreceptorAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleDespawned() override;

	void handleDied() override;

	void handleBackHome() override;

	void handleAttack(runtime::Ptr<Creature> creature) override;

private:
	void startSkillTask();

	void cancelTask();

	void startSkillEvent();

	runtime::Ptr<Creature> getRandomTarget();
};

} // namespace aion::gameserver::handlers::ai::instance::empyreanCrucible
