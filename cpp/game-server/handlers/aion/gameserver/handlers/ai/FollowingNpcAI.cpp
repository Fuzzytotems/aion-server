#include "aion/gameserver/handlers/ai/FollowingNpcAI.h"

#include "aion/gameserver/ai/handler/FollowEventHandler.h"

namespace aion::gameserver::handlers::ai {

AION_AI(FollowingNpcAI, "following");

// Java FollowingNpcAI.java:24-27
void FollowingNpcAI::handleFollowMe(Creature& creature) {
	gameserver::ai::handler::FollowEventHandler::follow(*this, creature);
}

// Java FollowingNpcAI.java:29-39
bool FollowingNpcAI::canHandleEvent(AIEventType eventType) {
	switch (eventType) {
		case AIEventType::CREATURE_MOVED:
			return getState() == AIState::FOLLOWING;
		case AIEventType::DIALOG_START:
		case AIEventType::DIALOG_FINISH:
			return getState() == AIState::FOLLOWING || GeneralNpcAI::canHandleEvent(eventType);
		default:
			break;
	}
	return GeneralNpcAI::canHandleEvent(eventType);
}

// Java FollowingNpcAI.java:41-48
void FollowingNpcAI::handleCreatureMoved(Creature& creature) {
	runtime::Ptr<VisibleObject> target = getOwner().getTarget();
	if (target != nullptr && creature.equals(*target)) {
		gameserver::ai::handler::FollowEventHandler::creatureMoved(*this, creature);
	} else if (target == nullptr) {
		gameserver::ai::handler::FollowEventHandler::stopFollow(*this, creature);
	}
}

// Java FollowingNpcAI.java:50-53
void FollowingNpcAI::handleStopFollowMe(Creature& creature) {
	gameserver::ai::handler::FollowEventHandler::stopFollow(*this, creature);
}

} // namespace aion::gameserver::handlers::ai
