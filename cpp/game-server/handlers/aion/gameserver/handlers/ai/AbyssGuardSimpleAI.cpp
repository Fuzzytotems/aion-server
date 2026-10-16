#include "aion/gameserver/handlers/ai/AbyssGuardSimpleAI.h"

#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/geo/GeoService.h"

namespace aion::gameserver::handlers::ai {

AION_AI(AbyssGuardSimpleAI, "simple_abyssguard");

// Java AbyssGuardSimpleAI.java:23-30: a switch with one arm and no default, so every other event falls through to the superclass
bool AbyssGuardSimpleAI::canHandleEvent(AIEventType eventType) {
	switch (eventType) {
		case AIEventType::CREATURE_MOVED:
			return getState() != AIState::FIGHT;
		default:
			break;
	}
	return AggressiveNpcAI::canHandleEvent(eventType);
}

// Java AbyssGuardSimpleAI.java:32-38
void AbyssGuardSimpleAI::handleCreatureSee(Creature& creature) {
	if (runtime::Ptr<Npc> npc = runtime::as<Npc>(creature))
		checkAggro(*npc); // custom checkAggro for npc vs npc
	else
		AggressiveNpcAI::handleCreatureSee(creature); // calls CreatureEventHandler.checkAggro
}

// Java AbyssGuardSimpleAI.java:40-46
void AbyssGuardSimpleAI::handleCreatureMoved(Creature& creature) {
	if (runtime::Ptr<Npc> npc = runtime::as<Npc>(creature))
		checkAggro(*npc); // custom checkAggro for npc vs npc
	else
		AggressiveNpcAI::handleCreatureMoved(creature); // calls CreatureEventHandler.checkAggro
}

// Java AbyssGuardSimpleAI.java:48-51
bool AbyssGuardSimpleAI::handleCreatureNeedsSupportByGuard(Creature& creature) {
	static_cast<void>(creature);
	return false;
}

// Java AbyssGuardSimpleAI.java:53-76
void AbyssGuardSimpleAI::checkAggro(Npc& npc) {
	if (isInState(AIState::FIGHT))
		return;

	if (isInState(AIState::RETURNING))
		return;

	Npc& guard = getOwner(); // Java: `Npc owner = getOwner();` (renamed: AbstractAI has a member of that name)
	if (npc.isDead() || !guard.canSee(runtime::Ptr<VisibleObject>(npc)))
		return;

	if (!guard.isEnemy(npc) || npc.getLevel() < 2)
		return;

	// ignore npcs which are under attack
	if (npc.getTarget())
		return;

	if (!guard.getPosition()->isMapRegionActive())
		return;

	// Java: PositionUtil.isInRange(owner, npc, owner.getAggroRange()) - the three-argument overload measures center to center
	if (PositionUtil::isInRange(guard, npc, static_cast<float>(guard.getAggroRange())) && GeoService::getInstance().canSee(guard, npc))
		onCreatureEvent(AIEventType::CREATURE_AGGRO, npc);
}

} // namespace aion::gameserver::handlers::ai
