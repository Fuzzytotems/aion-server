#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::abyss {

/**
 * The AI of an illusion gate ("illusion_gate"): from 10 seconds after its spawn, every 30 seconds two of the next three npc ids appear next to
 * it, until it dies or despawns.
 * <p>
 * Java: data/handlers/ai/instance/abyss/IllusionGateAI.java, @AIName("illusion_gate"). C++: the periodic task's lambda is pinned to this AI.
 *
 * @author Estrayl
 */
class IllusionGateAI : public NpcAI {
private:
	runtime::Field<runtime::FutureRef> spawnTask{}; // Java: private Future<?> spawnTask

public:
	explicit IllusionGateAI(Npc& owner) : NpcAI(owner) {}

protected:
	void handleSpawned() override;

	void handleDied() override;

	void handleDespawned() override;

private:
	void cancelTask();
};

} // namespace aion::gameserver::handlers::ai::instance::abyss
