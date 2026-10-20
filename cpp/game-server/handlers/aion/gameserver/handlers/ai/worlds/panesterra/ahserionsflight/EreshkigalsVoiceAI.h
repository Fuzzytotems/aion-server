#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::worlds::panesterra::ahserionsflight {

/**
 * Ereshkigal's voice ("ereshkigals_voice"): 30 s after its spawn it says 1501160 ("I won't forgive you, Fregion!!!") and despawns 2 s later.
 * <p>
 * Java: data/handlers/ai/worlds/panesterra/ahserionsflight/EreshkigalsVoiceAI.java, @AIName("ereshkigals_voice") (the marker is in the .cpp).
 * C++: the idle task is pinned to this AI, the despawn task to the owner.
 *
 * @author Estrayl
 */
class EreshkigalsVoiceAI : public NpcAI {
private:
	runtime::Field<runtime::FutureRef> idleTask{}; // Java: private Future<?> idleTask

public:
	explicit EreshkigalsVoiceAI(Npc& owner) : NpcAI(owner) {}

protected:
	void handleSpawned() override;

	void handleDespawned() override;
};

} // namespace aion::gameserver::handlers::ai::worlds::panesterra::ahserionsflight
