#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/siege/SiegeNpcAI.h"

namespace aion::gameserver::handlers::ai::siege {

/**
 * A siege incarnate ("incarnate"): the death of 259614 leaves the quest claw 701237 where it fell for five minutes.
 * <p>
 * Java: data/handlers/ai/siege/IncarnateAI.java, @AIName("incarnate") (the marker is in the .cpp). C++: the claw's despawn task is pinned to
 * this AI.
 *
 * @author Source
 */
class IncarnateAI : public SiegeNpcAI {
public:
	explicit IncarnateAI(Npc& owner) : SiegeNpcAI(owner) {}

protected:
	void handleDied() override;

private:
	void despawnClaw();
};

} // namespace aion::gameserver::handlers::ai::siege
