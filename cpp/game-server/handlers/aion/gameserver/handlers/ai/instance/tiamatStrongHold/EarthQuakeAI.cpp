#include "aion/gameserver/handlers/ai/instance/tiamatStrongHold/EarthQuakeAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/utils/PositionUtil.h"

namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold {

AION_AI(EarthQuakeAI, "earthquake");

// Java EarthQuakeAI.java:23-26
void EarthQuakeAI::handleCreatureSee(Creature& creature) {
	checkDistance(creature);
}

// Java EarthQuakeAI.java:28-31
void EarthQuakeAI::handleCreatureMoved(Creature& creature) {
	checkDistance(creature);
}

// Java EarthQuakeAI.java:33-39
void EarthQuakeAI::checkDistance(Creature& creature) {
	if (runtime::as<Player>(creature) != nullptr) { // Java: creature instanceof Player
		if (PositionUtil::isInRange(getOwner(), creature, 5) && !creature.getEffectController()->hasAbnormalEffect(20718)) {
			AIActions::useSkill(*this, 20718);
		}
	}
}

// Java EarthQuakeAI.java:41-45
void EarthQuakeAI::handleSpawned() {
	NpcAI::handleSpawned();
	despawn();
}

// Java EarthQuakeAI.java:47-49
void EarthQuakeAI::despawn() {
	ThreadPoolManager::getInstance().schedule({this}, [this] { getOwner().getController().delete_(); }, 9000);
}

// Java EarthQuakeAI.java:51-57
bool EarthQuakeAI::ask(AIQuestion question) {
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
