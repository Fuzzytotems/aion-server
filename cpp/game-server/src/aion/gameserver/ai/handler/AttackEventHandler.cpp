#include "aion/gameserver/ai/handler/AttackEventHandler.h"

#include <string>

#include "aion/gameserver/ai/AILogger.h"
#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/AISubState.h"
#include "aion/gameserver/ai/NpcAI.h"
#include "aion/gameserver/ai/event/AIEventType.h"
#include "aion/gameserver/ai/handler/ShoutEventHandler.h"
#include "aion/gameserver/ai/manager/AttackManager.h"
#include "aion/gameserver/ai/manager/EmoteManager.h"
#include "aion/gameserver/ai/manager/WalkManager.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/controllers/movement/NpcMoveController.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"

namespace aion::gameserver::ai::handler {

using event::AIEventType;
using manager::AttackManager;
using manager::EmoteManager;
using manager::WalkManager;
using model::gameobjects::Creature;
using model::gameobjects::Npc;
using skillengine::effect::AbnormalState;

void AttackEventHandler::onAttack(NpcAI& npcAI, runtime::Ptr<Creature> creature) {
	if (npcAI.isLogging()) {
		AILogger::info(npcAI, "onAttack");
	}
	if (!creature || creature->isDead()) {
		return;
	}
	// TODO lock or better switch
	if (npcAI.isInState(AIState::RETURNING)) {
		npcAI.getOwner().getMoveController()->abortMove();
		npcAI.setStateIfNot(AIState::IDLE);
		npcAI.onGeneralEvent(AIEventType::NOT_AT_HOME);
		return;
	}
	if (!npcAI.canThink()) {
		return;
	}
	if (npcAI.isInState(AIState::WALKING)) {
		WalkManager::stopWalking(npcAI);
	}
	npcAI.getOwner().getGameStats()->renewLastAttackedTime();
	bool allowFight = npcAI.getState() != AIState::FEAR && !npcAI.getOwner().getEffectController()->isAbnormalSet(AbnormalState::FEAR) &&
		npcAI.getState() != AIState::CONFUSE && !npcAI.getOwner().getEffectController()->isAbnormalSet(AbnormalState::CONFUSE);
	if (allowFight && npcAI.setStateIfNot(AIState::FIGHT)) {
		if (npcAI.isLogging())
			AILogger::info(npcAI, "onAttack() -> startAttacking");
		npcAI.setSubStateIfNot(AISubState::NONE);
		if (npcAI.getOwner().canSee(creature))
			npcAI.getOwner().setTarget(creature);
		AttackManager::startAttacking(npcAI);
		ShoutEventHandler::onAttackBegin(npcAI);
	}
}

void AttackEventHandler::onForcedAttack(NpcAI& npcAI) {
	onAttack(npcAI, runtime::cast<Creature>(npcAI.getOwner().getTarget()));
}

void AttackEventHandler::onAttackComplete(NpcAI& npcAI) {
	if (npcAI.isLogging()) {
		AILogger::info(npcAI, "onAttackComplete: " + std::to_string(npcAI.getOwner().getGameStats()->getLastAttackTimeDelta()));
	}
	npcAI.getOwner().getGameStats()->renewLastAttackTime();
	AttackManager::scheduleNextAttack(npcAI);
}

void AttackEventHandler::onFinishAttack(NpcAI& npcAI) {
	if (!npcAI.canThink()) {
		return;
	}
	if (npcAI.isLogging()) {
		AILogger::info(npcAI, "onFinishAttack");
	}
	Npc& npc = npcAI.getOwner();
	EmoteManager::emoteStopAttacking(npc);
	ShoutEventHandler::onAttackEnd(npcAI);
	npc.getController().loseAggro(true);
	npc.setSkillNumber(0);
}

} // namespace aion::gameserver::ai::handler
