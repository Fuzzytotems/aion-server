#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::siege {

/**
 * A healing spring of a fortress ("spring"): every 5 s, if a living creature of its race within 10 m is hurt and has no 19116 effect, it casts
 * 19116 on itself; no decay, rewards or loot.
 * <p>
 * Java: data/handlers/ai/siege/SpringAI.java, @AIName("spring") (the marker is in the .cpp). C++: the periodic task is pinned to this AI.
 *
 * @author xTz, Sykra
 */
class SpringAI : public NpcAI {
private:
	runtime::Field<runtime::FutureRef> healCheckTask{}; // Java: private Future<?> healCheckTask

public:
	explicit SpringAI(Npc& owner) : NpcAI(owner) {}

	void handleSpawned() override;

protected:
	void handleDied() override;

	void handleDespawned() override;

public:
	bool ask(AIQuestion question) override;

private:
	void cancelTasks();

	void checkForHeal();

	void doHeal();
};

} // namespace aion::gameserver::handlers::ai::siege
