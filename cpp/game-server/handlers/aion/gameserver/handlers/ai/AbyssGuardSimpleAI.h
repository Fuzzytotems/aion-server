#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

namespace aion::gameserver::handlers::ai {

/**
 * The AI of a guard that also fights npcs of an enemy tribe on its own ("simple_abyssguard", 859 npc templates: the Abyss guards, and the
 * quest-giving guards of the capitals and start maps such as Jucleas 203752 in Sanctum). Towards players it is an AggressiveNpcAI; an npc it
 * sees or that moves near it is checked by its own npc-vs-npc aggro rule instead of CreatureEventHandler.checkAggro, it ignores moves while it
 * fights, and it never answers another guard's call for support.
 * <p>
 * Java: data/handlers/ai/AbyssGuardSimpleAI.java, @AIName("simple_abyssguard") (the marker is in the .cpp).
 *
 * @author Rolandas, Neon
 */
class AbyssGuardSimpleAI : public AggressiveNpcAI {
public:
	explicit AbyssGuardSimpleAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	bool canHandleEvent(AIEventType eventType) override;

	void handleCreatureSee(Creature& creature) override;

	void handleCreatureMoved(Creature& creature) override;

	bool handleCreatureNeedsSupportByGuard(Creature& creature) override;

private:
	void checkAggro(Npc& npc);
};

} // namespace aion::gameserver::handlers::ai
