#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"
#include "aion/gameserver/ai/HpPhases.h"

#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

#include <vector>

namespace aion::gameserver::handlers::ai::instance::beshmundirTemple {

/**
 * The AI of the aethic field generator ("aethic_field_generator"): it never thinks; spawned it shields itself (19127); attacked it closes
 * door 471 and every 2 s goes home when no living player is 40 m near; at 75, 50 and 25 % hp it calls 2, 3 and 4 guards (216532); home or
 * dead it opens the door and takes the guards.
 * <p>
 * Java: data/handlers/ai/instance/beshmundirTemple/AethicFieldGeneratorAI.java, @AIName("aethic_field_generator") (the marker is in the .cpp).
 *
 * @author unknown
 */
class AethicFieldGeneratorAI : public GeneralNpcAI, public gameserver::ai::HpPhases::PhaseHandler {
private:
	const runtime::Ref<gameserver::ai::HpPhases> hpPhases = gameserver::ai::HpPhases::create(75, {50, 25}); // Java: = new HpPhases(75, 50, 25)
	runtime::AtomicBoolean isAggred{AION_LOCK_CLASS(AethicFieldGeneratorAI::isAggred)}; // Java: = new AtomicBoolean(false)
	runtime::Field<runtime::FutureRef> aggroTask_{}; // Java: aggroTask (the name clashes with aggroTask())

public:
	explicit AethicFieldGeneratorAI(Npc& owner) : GeneralNpcAI(owner) {}

	bool canThink() override;

protected:
	void handleSpawned() override;

	void handleAttack(runtime::Ptr<Creature> creature) override;

public:
	void handleHpPhase(int32_t phaseHpPercent) override;

private:
	void aggroTask();

	bool isInRangePlayer();

	void cancelAggroTask();

protected:
	void handleBackHome() override;

	void handleDied() override;

private:
	void deleteNpcs(const std::vector<runtime::Ptr<Npc>>& npcs);
};

} // namespace aion::gameserver::handlers::ai::instance::beshmundirTemple
