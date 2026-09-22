#include "aion/gameserver/ai/manager/AttackManager.h"

#include <string>

#include "aion/gameserver/ai/AILogger.h"
#include "aion/gameserver/ai/AISubState.h"
#include "aion/gameserver/ai/AttackIntention.h"
#include "aion/gameserver/ai/NpcAI.h"
#include "aion/gameserver/ai/event/AIEventType.h"
#include "aion/gameserver/ai/manager/EmoteManager.h"
#include "aion/gameserver/ai/manager/SimpleAttackManager.h"
#include "aion/gameserver/ai/manager/SkillAttackManager.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/controllers/attack/AggroTarget.h"
#include "aion/gameserver/controllers/movement/NpcMoveController.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/model/templates/world/AiInfo.h"
#include "aion/gameserver/model/templates/world/WorldMapTemplate.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::ai::manager {

using controllers::attack::AggroTarget;
using event::AIEventType;
using model::gameobjects::Creature;
using model::gameobjects::Npc;
using model::gameobjects::VisibleObject;
using utils::PositionUtil;

void AttackManager::startAttacking(NpcAI& npcAI) {
	if (npcAI.isLogging()) {
		AILogger::info(npcAI, "AttackManager: startAttacking");
	}
	npcAI.getOwner().getGameStats()->setFightStartingTime();
	runtime::Ptr<VisibleObject> target = npcAI.getOwner().getTarget();
	if (runtime::Ptr<Creature> creature = runtime::as<Creature>(target)) // may be null at this point (-> triggering then TARGET_GIVEUP)
		EmoteManager::emoteStartAttacking(npcAI.getOwner(), *creature);
	scheduleNextAttack(npcAI);
}

void AttackManager::scheduleNextAttack(NpcAI& npcAI) {
	if (npcAI.isLogging()) {
		AILogger::info(npcAI, "AttackManager: scheduleNextAttack");
	}
	// don't start attack while in casting substate
	AISubState subState = npcAI.getSubState();
	if (subState == AISubState::NONE) {
		chooseAttack(npcAI, npcAI.getOwner().getGameStats()->getNextAttackInterval());
	} else {
		if (npcAI.isLogging()) {
			AILogger::info(npcAI, "Will not choose attack in substate" + std::string(xml::enumName(subState)));
		}
	}
}

void AttackManager::chooseAttack(NpcAI& npcAI, int32_t delay) {
	AttackIntention attackIntention = npcAI.chooseAttackIntention();
	if (npcAI.isLogging()) {
		AILogger::info(npcAI, "AttackManager: chooseAttack " + std::string(xml::enumName(attackIntention)) + " delay " + std::to_string(delay));
	}
	if (!npcAI.canThink()) {
		return;
	}
	switch (attackIntention) {
		case AttackIntention::SIMPLE_ATTACK:
			SimpleAttackManager::performAttack(npcAI, delay);
			break;
		case AttackIntention::SKILL_ATTACK:
			SkillAttackManager::performAttack(npcAI, delay);
			break;
		case AttackIntention::FINISH_ATTACK:
			npcAI.think();
			break;
	}
}

void AttackManager::targetTooFar(NpcAI& npcAI) {
	Npc& npc = npcAI.getOwner();
	if (npcAI.isLogging()) {
		AILogger::info(npcAI, "AttackManager: attackTimeDelta " + std::to_string(npc.getGameStats()->getLastAttackTimeDelta()));
	}

	// switch target if there is more hated creature
	if (npc.getGameStats()->getLastChangeTargetTimeDelta() > 5) {
		runtime::Ptr<Creature> mostHated = npc.getAggroList().getTarget(AggroTarget::MOST_HATED);
		if (mostHated && !npc.isTargeting(mostHated->getObjectId())) {
			if (npcAI.isLogging()) {
				AILogger::info(npcAI, "AttackManager: switching target during chase");
			}
			npcAI.onCreatureEvent(AIEventType::TARGET_CHANGED, *mostHated);
			return;
		}
	}
	if (!npc.canSee(npc.getTarget())) {
		if (npcAI.setSubStateIfNot(AISubState::TARGET_LOST)) {
			utils::ThreadPoolManager::getInstance().schedule({&npc, &npcAI},
				[&npc, &npcAI] {
					if (npcAI.isInSubState(AISubState::TARGET_LOST) && npc.isSpawned() && !npc.isDead())
						npcAI.onGeneralEvent(AIEventType::TARGET_GIVEUP);
				},
				2000);
		}
		return;
	}
	if (checkGiveupDistance(npcAI)) {
		npcAI.onGeneralEvent(AIEventType::TARGET_GIVEUP);
		return;
	}
	if (npcAI.isMoveSupported()) {
		npc.getMoveController()->moveToTargetObject();
		return;
	}
	npcAI.onGeneralEvent(AIEventType::TARGET_GIVEUP);
}

bool AttackManager::checkGiveupDistance(NpcAI& npcAI) {
	Npc& npc = npcAI.getOwner();
	// if target run away too far
	runtime::Ptr<VisibleObject> target = npc.getTarget();
	if (target) {
		if (npcAI.isLogging())
			AILogger::info(npcAI, "AttackManager: distanceToTarget " + std::to_string(PositionUtil::getDistance(npc, *target, false)));
		int32_t maxChaseDistance =
			npc.isBoss() ? 50 : npc.getPosition()->getWorldMapInstance()->getTemplate()->getAiInfo()->getChaseTarget();
		if (!PositionUtil::isInRange(npc, *target, static_cast<float>(maxChaseDistance)))
			return true;
	}
	double distanceToHome = npc.getDistanceToSpawnLocation();
	// if npc is far away from home
	int32_t chaseHome = npc.isBoss() ? 150 : npc.getPosition()->getWorldMapInstance()->getTemplate()->getAiInfo()->getChaseHome();
	if (distanceToHome > chaseHome) {
		return true;
	}
	// start thinking about home after 100 meters and no attack for 10 seconds (only for default monsters)
	if (chaseHome <= 200) { // TODO: Check Client and use chase_user_by_trace value
		if ((npc.getGameStats()->getLastAttackTimeDelta() > 20 && npc.getGameStats()->getLastAttackedTimeDelta() > 20) ||
			(distanceToHome > chaseHome / 2 && npc.getGameStats()->getLastAttackedTimeDelta() > 10)) {
			return true;
		}
	}
	return false;
}

} // namespace aion::gameserver::ai::manager
