#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::stonespearReach {

/**
 * The AI of Stonespear Reach's kebbit 856303 ("stonespear_kebbit"): it never attacks and is deleted 15.5 s after its spawn, or at once when it
 * dies; no respawn, reward, loot or decay.
 * <p>
 * Java: data/handlers/ai/instance/stonespearReach/KebbitAI.java, @AIName("stonespear_kebbit"). C++: the despawn task's lambda is pinned to
 * this AI.
 *
 * @author Yeats
 */
class KebbitAI : public GeneralNpcAI {
private:
	runtime::Field<runtime::FutureRef> despawnTask{}; // Java: private Future<?> despawnTask

public:
	explicit KebbitAI(Npc& owner) : GeneralNpcAI(owner) {}

protected:
	void handleSpawned() override;

	void handleDied() override;

private:
	void cancelTask();

public:
	bool ask(AIQuestion question) override;

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;
};

} // namespace aion::gameserver::handlers::ai::instance::stonespearReach
