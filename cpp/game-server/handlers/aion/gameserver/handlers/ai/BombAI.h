#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

#include "aion/gameserver/model/templates/ai/fwd.h"
#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai {

/**
 * The AI of a bomb ("bomb"): its ai_templates bomb skill goes off cd + 2 s after the spawn, and the bomb is deleted 4 s after the skill's
 * duration; no decay, no reward.
 * <p>
 * Java: data/handlers/ai/BombAI.java, @AIName("bomb"). Java's `List<Future<?>> tasks` guarded by `synchronized (tasks)` is a runtime::ArrayList
 * under its own Monitor.
 */
class BombAI : public AggressiveNpcAI {
public:
	explicit BombAI(Npc& owner) : AggressiveNpcAI(owner) {}

	bool ask(AIQuestion question) override;

protected:
	void handleSpawned() override;
	void handleDied() override;
	void handleDespawned() override;

private:
	void addTask(runtime::FutureRef task);
	void cancelTasks();
	void useSkill(int32_t skill);

	/** Java: private BombTemplate template */
	runtime::Field<const model::templates::ai::BombTemplate*> template_{nullptr};
	/** Java: private final List<Future<?>> tasks = new ArrayList<>() */
	runtime::ArrayList<runtime::FutureRef> tasks{AION_LOCK_CLASS(BombAI::tasks)};
};

} // namespace aion::gameserver::handlers::ai
