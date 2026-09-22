#include "aion/gameserver/ai/handler/FollowEventHandler.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/ai/NpcAI.h"
#include "aion/gameserver/ai/event/AIEventType.h"
#include "aion/gameserver/ai/manager/EmoteManager.h"
#include "aion/gameserver/controllers/movement/NpcMoveController.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/utils/PositionUtil.h"

namespace aion::gameserver::ai::handler {

using event::AIEventType;
using manager::EmoteManager;
using model::gameobjects::Creature;
using model::gameobjects::VisibleObject;
using utils::PositionUtil;

void FollowEventHandler::follow(NpcAI& npcAI, Creature& creature) {
	if (npcAI.setStateIfNot(AIState::FOLLOWING)) {
		npcAI.getOwner().setTarget(runtime::Ptr<Creature>(creature));
		EmoteManager::emoteStartFollowing(npcAI.getOwner());
	}
}

void FollowEventHandler::creatureMoved(NpcAI& npcAI, Creature& creature) {
	if (npcAI.isInState(AIState::FOLLOWING)) {
		if (npcAI.getOwner().isTargeting(creature.getObjectId()) && !creature.isDead()) {
			checkFollowTarget(npcAI, creature);
		}
	}
}

void FollowEventHandler::checkFollowTarget(NpcAI& npcAI, Creature& creature) {
	if (!isInRange(npcAI, runtime::Ptr<Creature>(creature))) {
		npcAI.onGeneralEvent(AIEventType::TARGET_TOOFAR);
	}
}

bool FollowEventHandler::isInRange(AbstractAI& ai, runtime::Ptr<VisibleObject> object) {
	if (!object) {
		return false;
	}
	return PositionUtil::isInRange(ai.getOwner(), *object, 2, false);
}

void FollowEventHandler::stopFollow(NpcAI& npcAI, Creature& creature) {
	if (npcAI.setStateIfNot(AIState::IDLE)) {
		npcAI.getOwner().setTarget(nullptr);
		npcAI.getOwner().getMoveController()->abortMove();
		AIActions::scheduleRespawn(npcAI);
		AIActions::deleteOwner(npcAI);
	}
}

} // namespace aion::gameserver::ai::handler
