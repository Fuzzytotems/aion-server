#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::stonespearReach {

/**
 * The AI of Stonespear Reach's guardian stones ("stonespear_guardian_stone"): 55 s after its spawn a stone sends STR_MSG_OBJ_End and is
 * deleted; dead, it is deleted at once; no respawn, reward, loot or decay.
 * <p>
 * Java: data/handlers/ai/instance/stonespearReach/StonespearGuardianStoneAI.java, @AIName("stonespear_guardian_stone"). C++: the task's
 * lambda is pinned to this AI.
 *
 * @author Yeats
 */
class StonespearGuardianStoneAI : public NpcAI {
private:
	runtime::Field<runtime::FutureRef> task{}; // Java: private Future<?> task

public:
	explicit StonespearGuardianStoneAI(Npc& owner) : NpcAI(owner) {}

protected:
	void handleSpawned() override;

private:
	void startTask();

protected:
	void handleDied() override;

	void handleDespawned() override;

public:
	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::instance::stonespearReach
