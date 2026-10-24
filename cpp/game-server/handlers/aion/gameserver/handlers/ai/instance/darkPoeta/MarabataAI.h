#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::darkPoeta {

/**
 * The three Marabatas of Dark Poeta ("marabata"): each spawns its three boosters, and once attacked brings back the dead ones every 30 s.
 * <p>
 * Java: data/handlers/ai/instance/darkPoeta/MarabataAI.java, @AIName("marabata") (the marker is in the .cpp).
 *
 * @author xTz
 */
class MarabataAI : public AggressiveNpcAI {
private:
	runtime::Field<runtime::FutureRef> boosterLifeCheckTask{};
	runtime::AtomicBoolean isStarted{AION_LOCK_CLASS(MarabataAI::isStarted)}; // Java: = new AtomicBoolean()

public:
	explicit MarabataAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;

	void handleSpawned() override;

private:
	bool isBoosterAlive(int32_t boosterId);

	void cancelTask();

protected:
	void handleDied() override;

	void handleDespawned() override;
};

} // namespace aion::gameserver::handlers::ai::instance::darkPoeta
