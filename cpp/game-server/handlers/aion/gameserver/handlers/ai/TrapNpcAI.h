#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai {

/**
 * The AI of a trap ("trap": the Ranger's traps, the aetheric fields of quests, ...): hidden, it waits for an enemy of its creator to come in
 * attack range, then shows itself, casts a random skill of its list at it and is deleted 5 s later. A "shock trap" goes off as it spawns.
 * <p>
 * Java: data/handlers/ai/TrapNpcAI.java, @AIName("trap") (the marker is in the .cpp).
 */
class TrapNpcAI : public NpcAI {
private:
	runtime::Field<runtime::FutureRef> despawnTask{};

public:
	explicit TrapNpcAI(Npc& owner) : NpcAI(owner) {}

protected:
	void handleCreatureSee(Creature& creature) override;

	void handleCreatureMoved(Creature& creature) override;

private:
	void tryActivateTrap(Creature& creature);

protected:
	void handleSpawned() override;

private:
	void explode(Creature& creature);

public:
	bool isMoveSupported() override;

protected:
	bool canHandleEvent(AIEventType eventType) override;

public:
	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai
