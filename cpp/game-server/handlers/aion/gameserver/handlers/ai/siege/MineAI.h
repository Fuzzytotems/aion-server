#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/siege/SiegeNpcAI.h"

namespace aion::gameserver::handlers::ai::siege {

/**
 * A siege mine ("siege_mine"): the first creature it aggroes makes it cast 18407, and it deletes itself 1.5 s later.
 * <p>
 * Java: data/handlers/ai/siege/MineAI.java, @AIName("siege_mine") (the marker is in the .cpp). C++: the delete task is pinned to this AI.
 *
 * @author Source
 */
class MineAI : public SiegeNpcAI {
public:
	explicit MineAI(Npc& owner) : SiegeNpcAI(owner) {}

protected:
	void handleCreatureAggro(Creature& creature) override;
};

} // namespace aion::gameserver::handlers::ai::siege
