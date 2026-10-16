#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/runtime/fields/Atomic.h"

namespace aion::gameserver::handlers::ai::instance::raksang {

/**
 * The AI of the Flamelord's scalding executors ("scalding_executor"): it does not think, it follows; arrived at its target (one of the four
 * fields) it stops, casts 19928 on it and 5.3 s later sets the field on fire and goes; no decay, respawn or reward; immune to abnormal
 * states; dead it goes.
 * <p>
 * Java: data/handlers/ai/instance/raksang/ScaldingExecutorAI.java, @AIName("scalding_executor") (the marker is in the .cpp).
 *
 * @author xTz
 */
class ScaldingExecutorAI : public AggressiveNpcAI {
private:
	runtime::AtomicBoolean isDestroyed{AION_LOCK_CLASS(ScaldingExecutorAI::isDestroyed)}; // Java: = new AtomicBoolean()

public:
	explicit ScaldingExecutorAI(Npc& owner) : AggressiveNpcAI(owner) {}

	bool canThink() override;

protected:
	void handleSpawned() override;

	void handleMoveArrived() override;

public:
	bool ask(AIQuestion question) override;

protected:
	void handleDied() override;
};

} // namespace aion::gameserver::handlers::ai::instance::raksang
