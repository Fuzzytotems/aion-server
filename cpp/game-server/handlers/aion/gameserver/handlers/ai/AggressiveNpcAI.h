#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"

namespace aion::gameserver::handlers::ai {

/**
 * The AI of an aggressive NPC ("aggressive"): a GeneralNpcAI that also starts fights - it checks every creature it sees for aggro and answers
 * a guard's call for support.
 * <p>
 * Java: data/handlers/ai/AggressiveNpcAI.java, @AIName("aggressive") (the marker is in the .cpp).
 *
 * @author ATracer
 */
class AggressiveNpcAI : public GeneralNpcAI {
public:
	explicit AggressiveNpcAI(Npc& owner) : GeneralNpcAI(owner) {}

protected:
	void handleCreatureSee(Creature& creature) override;

	void handleCreatureAggro(Creature& creature) override;

	bool handleCreatureNeedsSupportByGuard(Creature& creature) override;
};

} // namespace aion::gameserver::handlers::ai
