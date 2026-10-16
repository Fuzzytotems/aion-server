#include "aion/gameserver/handlers/ai/instance/tiamatStrongHold/SlowTimeAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/utils/PositionUtil.h"

namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold {

AION_AI(SlowTimeAI, "slowedtime");

// Java SlowTimeAI.java:22-25
void SlowTimeAI::handleCreatureSee(Creature& creature) {
	checkDistance(*this, creature);
}

// Java SlowTimeAI.java:27-30
void SlowTimeAI::handleCreatureMoved(Creature& creature) {
	checkDistance(*this, creature);
}

// Java SlowTimeAI.java:32-38
void SlowTimeAI::checkDistance(NpcAI& /*ai*/, Creature& creature) {
	if (runtime::as<Player>(creature) != nullptr) { // Java: creature instanceof Player
		if (PositionUtil::isInRange(getOwner(), creature, 50) && !creature.getEffectController()->hasAbnormalEffect(20728)) {
			AIActions::useSkill(*this, 20728);
		}
	}
}

// Java SlowTimeAI.java:40-46
bool SlowTimeAI::ask(AIQuestion question) {
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
