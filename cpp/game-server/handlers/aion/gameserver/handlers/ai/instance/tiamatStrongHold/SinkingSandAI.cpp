#include "aion/gameserver/handlers/ai/instance/tiamatStrongHold/SinkingSandAI.h"

#include "aion/gameserver/ai/AIActions.h"

namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold {

AION_AI(SinkingSandAI, "sinkingsand");

// Java SinkingSandAI.java:19-23
void SinkingSandAI::handleSpawned() {
	NpcAI::handleSpawned();
	useskill();
}

// Java SinkingSandAI.java:25-40 (the anonymous Runnables: tasks pinning this)
void SinkingSandAI::useskill() {
	ThreadPoolManager::getInstance().schedule({this}, [this] {
		AIActions::useSkill(*this, 20723);
		ThreadPoolManager::getInstance().schedule({this}, [this] { AIActions::deleteOwner(*this); }, 1000);
	}, 3000);
}

} // namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold
