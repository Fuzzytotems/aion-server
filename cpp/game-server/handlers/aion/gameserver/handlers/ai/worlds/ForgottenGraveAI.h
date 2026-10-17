#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"

#include "aion/gameserver/runtime/fields/Atomic.h"

namespace aion::gameserver::handlers::ai::worlds {

/**
 * A forgotten grave ("forgottengrave"): it does not think; its first death brings two 283906 around it, each deleted after five minutes if
 * still alive.
 * <p>
 * Java: data/handlers/ai/worlds/ForgottenGraveAI.java, @AIName("forgottengrave") (the marker is in the .cpp). C++: the despawn tasks are
 * pinned to the spawned npcs (their controllers hold them, TaskId.DESPAWN).
 *
 * @author Tibald
 */
class ForgottenGraveAI : public GeneralNpcAI {
private:
	runtime::AtomicBoolean isSpawned{AION_LOCK_CLASS(ForgottenGraveAI::isSpawned)}; // Java: = new AtomicBoolean()

public:
	explicit ForgottenGraveAI(Npc& owner) : GeneralNpcAI(owner) {}

	bool canThink() override;

protected:
	void handleDied() override;

private:
	void scheduleAddDespawn(Npc& npc);
};

} // namespace aion::gameserver::handlers::ai::worlds
