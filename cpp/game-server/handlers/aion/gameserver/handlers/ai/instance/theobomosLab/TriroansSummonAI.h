#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sync/Monitor.h"

namespace aion::gameserver::handlers::ai::instance::theobomosLab {

/**
 * The AI of Triroan's element summons in Theobomos Lab ("triroan_summon": fire 280975, water 280976, earth 280977, wind 280978): they do not
 * think; walking to the boss, at their step (3, 4, 2, 5) they stop once, make Triroan (214669) cast their helper skill (18493, 18492, 18485,
 * 18491) - at once when he stands at their element's pillar, else when he stands at none of the four (every 5 s while he casts) - and are
 * deleted 3 s later.
 * <p>
 * Java: data/handlers/ai/instance/theobomosLab/TriroansSummonAI.java, @AIName("triroan_summon") (the marker is in the .cpp). Java's
 * synchronized useSkill is the monitor member.
 *
 * @author Ritsu
 */
class TriroansSummonAI : public AggressiveNpcAI {
private:
	runtime::AtomicBoolean isDestroyed{AION_LOCK_CLASS(TriroansSummonAI::isDestroyed), false}; // Java: = new AtomicBoolean(false)
	runtime::Field<int32_t> walkPosition{};
	runtime::Field<int32_t> helperSkill{};
	runtime::Monitor monitor{AION_LOCK_CLASS(TriroansSummonAI::monitor)}; // fieldmap: Java's synchronized useSkill

public:
	explicit TriroansSummonAI(Npc& owner) : AggressiveNpcAI(owner) {}

	bool canThink() override;

protected:
	void handleSpawned() override;

	void handleMoveArrived() override;

private:
	void useSkill();

	void checkSkillUse(runtime::Ptr<Npc> boss);

	bool checkLocation(Npc& npc);

public:
	int32_t checkDistance();

private:
	void startDespawnTask();
};

} // namespace aion::gameserver::handlers::ai::instance::theobomosLab
