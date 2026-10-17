#include "aion/gameserver/handlers/ai/instance/tiamatStrongHold/ChantraRingsAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/utils/PositionUtil.h"

namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold {

AION_AI(ChantraRingsAI, "chantrarings");

// Java ChantraRingsAI.java:23-26
void ChantraRingsAI::handleCreatureSee(Creature& creature) {
	checkDistance(*this, creature);
}

// Java ChantraRingsAI.java:28-31
void ChantraRingsAI::handleCreatureMoved(Creature& creature) {
	checkDistance(*this, creature);
}

// Java ChantraRingsAI.java:33-43
void ChantraRingsAI::checkDistance(NpcAI& /*ai*/, Creature& creature) {
	int32_t debuff = getOwner().getNpcId() == 283172 ? 20735 : 20734; // 4.0
	if (runtime::as<Player>(creature) != nullptr) { // Java: creature instanceof Player
		if ((getOwner().getNpcId() == 283172 && PositionUtil::isInRangeLimited(getOwner(), creature, 10, 18)) // 4.0
			|| (getOwner().getNpcId() == 283171 && PositionUtil::isInRangeLimited(getOwner(), creature, 18, 25)) // 4.0
			|| (getOwner().getNpcId() == 283171 && PositionUtil::isInRangeLimited(getOwner(), creature, 0, 10))) { // 4.0
			if (!creature.getEffectController()->hasAbnormalEffect(debuff))
				AIActions::useSkill(*this, debuff);
		}
	}
}

// Java ChantraRingsAI.java:45-49
void ChantraRingsAI::handleSpawned() {
	NpcAI::handleSpawned();
	despawn();
}

// Java ChantraRingsAI.java:51-53
void ChantraRingsAI::despawn() {
	ThreadPoolManager::getInstance().schedule({this}, [this] { getOwner().getController().delete_(); }, 20000);
}

// Java ChantraRingsAI.java:55-61
bool ChantraRingsAI::ask(AIQuestion question) {
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
