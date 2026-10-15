#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

#include <vector>

#include "aion/gameserver/ai/HpPhases.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::raksang {

/**
 * The AI of Raksha in Raksang Ruins ("raksha"): the first attack is announced; below 75 % HP every 15 s from 3 s she casts 19938 and drops
 * rubble on the living players she knows (all of up to three, else 3 or more at random) 3 s later; her death opens the exit.
 * <p>
 * Java: data/handlers/ai/instance/raksang/RakshaAI.java, @AIName("raksha") (the marker is in the .cpp).
 *
 * @author xTz
 */
class RakshaAI : public AggressiveNpcAI, public gameserver::ai::HpPhases::PhaseHandler {
private:
	const runtime::Ref<gameserver::ai::HpPhases> hpPhases = gameserver::ai::HpPhases::create(75); // Java: = new HpPhases(75)
	runtime::AtomicBoolean isAggred{AION_LOCK_CLASS(RakshaAI::isAggred), false}; // Java: = new AtomicBoolean(false)
	runtime::Field<runtime::FutureRef> phaseTask{};

public:
	explicit RakshaAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleDespawned() override;

	void handleDied() override;

	void handleAttack(runtime::Ptr<Creature> creature) override;

public:
	void handleHpPhase(int32_t phaseHpPercent) override;

private:
	void startPhaseTask();

	void spawnRubble(Player& player);

	std::vector<runtime::Ptr<Player>> getLifedPlayers();

	void cancelPhaseTask();

protected:
	void handleBackHome() override;
};

} // namespace aion::gameserver::handlers::ai::instance::raksang
