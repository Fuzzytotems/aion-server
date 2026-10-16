#include "aion/gameserver/ai/handler/TargetEventHandler.h"

#include "aion/gameserver/ai/AILogger.h"
#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/AISubState.h"
#include "aion/gameserver/ai/NpcAI.h"
#include "aion/gameserver/ai/event/AIEventType.h"
#include "aion/gameserver/ai/handler/CreatureEventHandler.h"
#include "aion/gameserver/ai/manager/AttackManager.h"
#include "aion/gameserver/ai/manager/FollowManager.h"
#include "aion/gameserver/ai/manager/WalkManager.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/controllers/movement/NpcMoveController.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::ai::handler {

using event::AIEventType;
using manager::AttackManager;
using manager::FollowManager;
using manager::WalkManager;
using model::gameobjects::Creature;
using model::gameobjects::VisibleObject;

void TargetEventHandler::onTargetReached(NpcAI& npcAI) {
	if (npcAI.isLogging()) {
		AILogger::info(npcAI, "onTargetReached");
	}

	AIState currentState = npcAI.getState();
	switch (currentState) {
		case AIState::FIGHT:
			npcAI.getOwner().getMoveController()->abortMove();
			AttackManager::scheduleNextAttack(npcAI);
			break;
		case AIState::RETURNING:
			npcAI.getOwner().getMoveController()->abortMove();
			if (npcAI.getOwner().isAtSpawnLocation())
				npcAI.onGeneralEvent(AIEventType::BACK_HOME);
			else {
				npcAI.setStateIfNot(AIState::IDLE);
				npcAI.onGeneralEvent(AIEventType::NOT_AT_HOME);
			}
			break;
		case AIState::FOLLOWING:
		case AIState::CONFUSE:
		case AIState::FEAR:
			npcAI.getOwner().getMoveController()->abortMove();
			break;
		case AIState::WALKING:
			WalkManager::targetReached(npcAI);
			checkAggro(npcAI);
			break;
		case AIState::FORCED_WALKING:
			WalkManager::targetReached(npcAI);
			break;
		default:
			break;
	}
}

void TargetEventHandler::onTargetTooFar(NpcAI& npcAI) {
	if (npcAI.isLogging()) {
		AILogger::info(npcAI, "onTargetTooFar");
	}
	switch (npcAI.getState()) {
		case AIState::FIGHT:
			AttackManager::targetTooFar(npcAI);
			break;
		case AIState::FOLLOWING:
			FollowManager::targetTooFar(npcAI);
			break;
		case AIState::CONFUSE:
		case AIState::FEAR:
			break;
		default:
			if (npcAI.isLogging()) {
				AILogger::info(npcAI, "default onTargetTooFar");
			}
	}
}

void TargetEventHandler::onTargetGiveup(NpcAI& npcAI) {
	if (npcAI.isLogging()) {
		AILogger::info(npcAI, "onTargetGiveup");
	}
	runtime::Ptr<VisibleObject> target = npcAI.getOwner().getTarget();
	if (target) {
		if (npcAI.getSubState() == AISubState::TARGET_LOST)
			npcAI.setSubStateIfNot(AISubState::NONE);
		npcAI.getOwner().getAggroList().stopHating(*target);
	}
	if (npcAI.isMoveSupported()) {
		npcAI.getOwner().getMoveController()->abortMove();
	}
	if (!npcAI.isDead())
		npcAI.think();
}

void TargetEventHandler::onTargetChange(NpcAI& npcAI, Creature& creature) {
	if (npcAI.isLogging()) {
		AILogger::info(npcAI, "onTargetChange");
	}
	if (npcAI.isInState(AIState::FIGHT)) {
		npcAI.getOwner().setTarget(runtime::Ptr<Creature>(creature));
		AttackManager::scheduleNextAttack(npcAI);
	}
}

void TargetEventHandler::checkAggro(NpcAI& npcAI) {
	npcAI.getOwner().getKnownList().forEachObject([&npcAI](VisibleObject& obj) {
		if (runtime::Ptr<Creature> creature = runtime::as<Creature>(obj))
			CreatureEventHandler::checkAggro(npcAI, *creature);
	});
}

} // namespace aion::gameserver::ai::handler
