#include "aion/gameserver/handlers/ai/instance/tiamatStrongHold/DistortedSpaceAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/controllers/NpcController.h"

namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold {

AION_AI(DistortedSpaceAI, "distortedspace");

// Java DistortedSpaceAI.java:24-28
void DistortedSpaceAI::handleSpawned() {
	NpcAI::handleSpawned();
	useskill();
}

// Java DistortedSpaceAI.java:30-42
void DistortedSpaceAI::useskill() {
	task.set(ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] {
		if (getOwner().getNpcId() == 283097)
			AIActions::useSkill(*this, 20740);
	}, 500, 2000));

	ThreadPoolManager::getInstance().schedule({this}, [this] {
		cancelTask();
		if (getOwner().getNpcId() == 283097)
			AIActions::useSkill(*this, 20742);
		getOwner().getController().die();
	}, 8000);
}

// Java DistortedSpaceAI.java:44-48
void DistortedSpaceAI::cancelTask() {
	runtime::FutureRef current = task.get();
	if (current != nullptr && !current->isCancelled()) {
		current->cancel(true);
	}
}

// Java DistortedSpaceAI.java:50-55
void DistortedSpaceAI::handleDied() {
	NpcAI::handleDied();
	cancelTask();
	AIActions::deleteOwner(*this);
}

// Java DistortedSpaceAI.java:57-61
void DistortedSpaceAI::handleDespawned() {
	NpcAI::handleDespawned();
	cancelTask();
}

// Java DistortedSpaceAI.java:63-69
bool DistortedSpaceAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::ALLOW_DECAY:
		case AIQuestion::ALLOW_RESPAWN:
		case AIQuestion::REWARD_AP_XP_DP_LOOT:
			return false;
		default:
			return NpcAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold
