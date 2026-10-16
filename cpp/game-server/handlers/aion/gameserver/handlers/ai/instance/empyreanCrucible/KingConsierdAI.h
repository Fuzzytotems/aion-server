#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/ai/HpPhases.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include <vector>

namespace aion::gameserver::handlers::ai::instance::empyreanCrucible {

/**
 * King Consierd of Empyrean Crucible's stage 9 ("king_consierd"): the opening skills, the blood thirst after 3 minutes, the bomb skills
 * from 75 % (with two 282378 below 50 %) and the last skill at 25 %.
 * <p>
 * Java: data/handlers/ai/instance/empyreanCrucible/KingConsierdAI.java, @AIName("king_consierd") (the marker is in the .cpp).
 *
 * @author Luzien
 */
class KingConsierdAI : public AggressiveNpcAI, public gameserver::ai::HpPhases::PhaseHandler {
private:
	const runtime::Ref<gameserver::ai::HpPhases> hpPhases = gameserver::ai::HpPhases::create(75, {25}); // Java: = new HpPhases(75, 25)
	runtime::AtomicBoolean isHome{AION_LOCK_CLASS(KingConsierdAI::isHome), true}; // Java: = new AtomicBoolean(true)
	runtime::Field<runtime::FutureRef> eventTask{};
	runtime::Field<runtime::FutureRef> skillTask{};

public:
	explicit KingConsierdAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleDespawned() override;

	void handleDied() override;

	void handleBackHome() override;

	void handleAttack(runtime::Ptr<Creature> creature) override;

public:
	void handleHpPhase(int32_t phaseHpPercent) override;

private:
	void startBloodThirstTask();

	void startSkillTask();

	void cancelTasks();

	void despawnNpcs(const std::vector<runtime::Ptr<Npc>>& npcs);
};

} // namespace aion::gameserver::handlers::ai::instance::empyreanCrucible
