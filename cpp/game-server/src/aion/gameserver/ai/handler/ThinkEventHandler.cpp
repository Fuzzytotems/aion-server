#include "aion/gameserver/ai/handler/ThinkEventHandler.h"

#include <string>

#include "aion/gameserver/ai/AILogger.h"
#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/AISubState.h"
#include "aion/gameserver/ai/NpcAI.h"
#include "aion/gameserver/ai/event/AIEventType.h"
#include "aion/gameserver/ai/manager/AttackManager.h"
#include "aion/gameserver/ai/manager/WalkManager.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/controllers/movement/NpcMoveController.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_HEADING_UPDATE.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::ai::handler {

using event::AIEventType;
using manager::AttackManager;
using manager::WalkManager;
using model::gameobjects::Creature;
using model::gameobjects::Npc;
using network::aion::serverpackets::SM_HEADING_UPDATE;
using utils::PacketSendUtility;

void ThinkEventHandler::onThink(NpcAI& npcAI) {
	if (npcAI.isDead()) {
		AILogger::info(npcAI, "can't think in dead state");
		return;
	}
	if (!npcAI.setThinking()) {
		AILogger::info(npcAI, "skipped onThink because AI is already thinking");
		return;
	}
	// Java: try { ... } finally { npcAI.unsetThinking(); }
	struct UnsetThinking {
		NpcAI& ai;
		~UnsetThinking() { ai.unsetThinking(); }
	} unsetThinking{npcAI};
	if (!npcAI.getOwner().getPosition()->isMapRegionActive() || npcAI.getSubState() == AISubState::FREEZE) {
		thinkInInactiveRegion(npcAI);
		return;
	}
	if (npcAI.isLogging()) {
		AILogger::info(npcAI, "think in ai state: " + std::string(xml::enumName(npcAI.getState())));
	}
	switch (npcAI.getState()) {
		case AIState::FIGHT:
			thinkAttack(npcAI);
			break;
		case AIState::IDLE:
			thinkIdle(npcAI);
			break;
		default:
			break;
	}
}

void ThinkEventHandler::thinkInInactiveRegion(NpcAI& npcAI) {
	if (npcAI.isInState(AIState::WALKING)) {
		WalkManager::stopWalking(npcAI);
		return;
	}
	if (!npcAI.canThink()) {
		return;
	}
	if (npcAI.isLogging()) {
		AILogger::info(npcAI, "think (inactive region) in ai state: " + std::string(xml::enumName(npcAI.getState())));
	}
	switch (npcAI.getState()) {
		case AIState::FIGHT:
			thinkAttack(npcAI);
			break;
		default:
			if (!npcAI.getOwner().isAtSpawnLocation()) {
				npcAI.onGeneralEvent(AIEventType::NOT_AT_HOME);
			}
	}
}

void ThinkEventHandler::thinkAttack(NpcAI& npcAI) {
	Npc& npc = npcAI.getOwner();
	runtime::Ptr<Creature> target = runtime::as<Creature>(npc.getTarget());
	if (target && npc.getAggroList().isHating(*target)) {
		AttackManager::scheduleNextAttack(npcAI);
	} else {
		npcAI.setSubStateIfNot(AISubState::NONE);
		npc.clearQueuedSkills();
		npc.getGameStats()->setLastSkill(nullptr);
		npc.getGameStats()->resetFightStats();
		npcAI.onGeneralEvent(AIEventType::ATTACK_FINISH);
		npcAI.onGeneralEvent(npc.isAtSpawnLocation() ? AIEventType::BACK_HOME : AIEventType::NOT_AT_HOME);
	}
}

void ThinkEventHandler::thinkIdle(NpcAI& npcAI) {
	if (npcAI.isMoveSupported() && npcAI.getOwner().isWalker())
		WalkManager::startWalking(npcAI);
	else if (shouldResetHeading(npcAI)) {
		utils::ThreadPoolManager::getInstance().schedule({&npcAI},
			[&npcAI] {
				if (shouldResetHeading(npcAI)) {
					npcAI.getPosition()->setH(npcAI.getOwner().getSpawn()->getHeading());
					PacketSendUtility::broadcastPacket(npcAI.getOwner(), SM_HEADING_UPDATE(npcAI.getOwner()));
				}
			},
			500);
	}
}

bool ThinkEventHandler::shouldResetHeading(NpcAI& npcAI) {
	runtime::Ptr<model::templates::spawns::SpawnTemplate> spawn = npcAI.getOwner().getSpawn();
	return !npcAI.getTarget() && spawn && !npcAI.getOwner().getMoveController()->isInMove() &&
		npcAI.getPosition()->getHeading() != spawn->getHeading() && npcAI.getOwner().isAtSpawnLocation();
}

} // namespace aion::gameserver::ai::handler
