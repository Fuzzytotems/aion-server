#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::darkPoeta {

/**
 * Dark Poeta's telepathy controller ("telepathy_controller"): once attacked, every minute an add (281150 or 281334) that hates the
 * players it knows.
 * <p>
 * Java: data/handlers/ai/instance/darkPoeta/TelepathyControllerAI.java, @AIName("telepathy_controller") (the marker is in the .cpp).
 *
 * @author Ritsu, Estrayl
 */
class TelepathyControllerAI : public AggressiveNpcAI {
private:
	runtime::Field<runtime::FutureRef> spawnTask{};
	runtime::AtomicBoolean isAggred{AION_LOCK_CLASS(TelepathyControllerAI::isAggred)}; // Java: = new AtomicBoolean()

public:
	explicit TelepathyControllerAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;

private:
	void cancelTask();

protected:
	void handleBackHome() override;

	void handleDied() override;

	void handleDespawned() override;
};

} // namespace aion::gameserver::handlers::ai::instance::darkPoeta
