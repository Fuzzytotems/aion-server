#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/sync/Monitor.h"

namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold {

/**
 * The AI of the captured Drakan scientists in Tiamat Stronghold ("captured_drakan_scientist"): the first player it sees makes it watch the
 * guarding eyes (219390) within 25 m; when two of them have died it walks off after 1-2 s, and 9 s later every player inside moves his
 * quest (30708 Elyos, 30758 Asmodian) one step, up to 5, and the scientist is deleted.
 * <p>
 * Java: data/handlers/ai/instance/tiamatStrongHold/CapturedDrakanScientistAI.java, @AIName("captured_drakan_scientist") (the marker is in
 * the .cpp).
 *
 * @author Estrayl
 */
class CapturedDrakanScientistAI : public GeneralNpcAI {
private:
	runtime::AtomicInteger deadGuardingEyes{AION_LOCK_CLASS(CapturedDrakanScientistAI::deadGuardingEyes)};
	runtime::AtomicBoolean isActivated{AION_LOCK_CLASS(CapturedDrakanScientistAI::isActivated)};
	/** Java's synchronized (qs): nothing else locks a QuestState, so one lock for these sections excludes as Java's per-state lock does */
	static inline runtime::Monitor questLock{AION_LOCK_CLASS(CapturedDrakanScientistAI::questLock)};

public:
	explicit CapturedDrakanScientistAI(Npc& owner) : GeneralNpcAI(owner) {}

protected:
	void handleCreatureSee(Creature& creature) override;

private:
	void handleObservedNpcDied();

	void startWalk();

	void handleNpcEscaping();

	void handleQuestUpdate();

	void updateQuestEntryIfPossible(Player& player);
};

} // namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold
