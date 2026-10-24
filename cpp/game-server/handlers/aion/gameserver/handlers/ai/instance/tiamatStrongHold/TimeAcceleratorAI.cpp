#include "aion/gameserver/handlers/ai/instance/tiamatStrongHold/TimeAcceleratorAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/utils/PositionUtil.h"

namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold {

AION_AI(TimeAcceleratorAI, "timeaccelerator");

// Java TimeAcceleratorAI.java:22-25
void TimeAcceleratorAI::handleCreatureSee(Creature& creature) {
	checkDistance(creature);
}

// Java TimeAcceleratorAI.java:27-30
void TimeAcceleratorAI::handleCreatureMoved(Creature& creature) {
	checkDistance(creature);
}

// Java TimeAcceleratorAI.java:32-35
void TimeAcceleratorAI::checkDistance(Creature& creature) {
	if (PositionUtil::isInRange(getOwner(), creature, 5) && !creature.getEffectController()->hasAbnormalEffect(20727))
		AIActions::useSkill(*this, 20727);
}

// Java TimeAcceleratorAI.java:37-41
void TimeAcceleratorAI::handleSpawned() {
	NpcAI::handleSpawned();
	despawn();
}

// Java TimeAcceleratorAI.java:43-45
void TimeAcceleratorAI::despawn() {
	ThreadPoolManager::getInstance().schedule({this}, [this] { getOwner().getController().delete_(); }, 20000);
}

// Java TimeAcceleratorAI.java:47-53
bool TimeAcceleratorAI::ask(AIQuestion question) {
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
