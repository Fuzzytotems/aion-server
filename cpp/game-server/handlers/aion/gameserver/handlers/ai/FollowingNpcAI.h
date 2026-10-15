#pragma once

#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"

namespace aion::gameserver::handlers::ai {

/**
 * The AI of an npc that follows a creature ("following"): GeneralNpcAI with the follow events of FollowEventHandler.
 * <p>
 * Java: data/handlers/ai/FollowingNpcAI.java, @AIName("following") (the marker is in the .cpp).
 *
 * @author ATracer
 */
class FollowingNpcAI : public GeneralNpcAI {
public:
	explicit FollowingNpcAI(Npc& owner) : GeneralNpcAI(owner) {}

protected:
	void handleFollowMe(Creature& creature) override;
	bool canHandleEvent(AIEventType eventType) override;
	void handleCreatureMoved(Creature& creature) override;
	void handleStopFollowMe(Creature& creature) override;
};

} // namespace aion::gameserver::handlers::ai
