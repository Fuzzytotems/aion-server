#include "aion/gameserver/handlers/ai/instance/nightmareCircus/EnragedNightmareAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/ai/manager/EmoteManager.h"
#include "aion/gameserver/ai/manager/WalkManager.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/movement/NpcMoveController.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::nightmareCircus {

AION_AI(EnragedNightmareAI, "enragednightmare");

// Java EnragedNightmareAI.java:35-38
void EnragedNightmareAI::handleCreatureSee(Creature& creature) {
	useSkill(*this, boss.get());
}

// Java EnragedNightmareAI.java:40-43
void EnragedNightmareAI::handleCreatureMoved(Creature& creature) {
	useSkill(*this, boss.get());
}

// Java EnragedNightmareAI.java:45-49
void EnragedNightmareAI::cancelTask() {
	runtime::FutureRef task = skillTask.get();
	if (task != nullptr && !task->isCancelled()) {
		task->cancel(true);
	}
}

// Java EnragedNightmareAI.java:51-55
void EnragedNightmareAI::handleDied() {
	cancelTask();
	AggressiveNpcAI::handleDied();
}

// Java EnragedNightmareAI.java:57-61
void EnragedNightmareAI::handleDespawned() {
	cancelTask();
	AggressiveNpcAI::handleDespawned();
}

// Java EnragedNightmareAI.java:63-81
void EnragedNightmareAI::handleSpawned() {
	AggressiveNpcAI::handleSpawned();

	if (getOwner().getPosition()->getX() == 521.585f && getOwner().getPosition()->getY() == 510.16528f) {
		getOwner().getAi().setStateIfNot(AIState::WALKING);
		WalkManager::startWalking(dynamic_cast<gameserver::ai::NpcAI&>(getOwner().getAi())); // Java: (NpcAI) getOwner().getAi()
		getOwner().setState(CreatureState::ACTIVE, true);
		getOwner().getMoveController()->moveToPoint(521.60913f, 551.00684f, 198.75f);
		PacketSendUtility::broadcastPacket(getOwner(), SM_EMOTION(getOwner(), EmotionType::CHANGE_SPEED, 0, getOwner().getObjectId()));
	} else {
		getOwner().getAi().setStateIfNot(AIState::WALKING);
		WalkManager::startWalking(dynamic_cast<gameserver::ai::NpcAI&>(getOwner().getAi())); // Java: (NpcAI) getOwner().getAi()
		getOwner().setState(CreatureState::ACTIVE, true);
		getOwner().getMoveController()->moveToPoint(525.3831f, 585.3808f, 199.0476f);
		PacketSendUtility::broadcastPacket(getOwner(), SM_EMOTION(getOwner(), EmotionType::CHANGE_SPEED, 0, getOwner().getObjectId()));
	}

}

// Java EnragedNightmareAI.java:83-94 (Java's parameters are unused)
void EnragedNightmareAI::useSkill(gameserver::ai::NpcAI& ai, runtime::Ptr<Creature> creature) {
	boss = getPosition()->getWorldMapInstance()->getNpc(233467);
	runtime::Ptr<Npc> current = boss.get();
	if (current != nullptr && !current->isDead()) {
		if (current->getLifeStats()->getHpPercentage() < 100) {
			EmoteManager::emoteStopAttacking(getOwner());
			SkillEngine::getInstance().getSkill(getOwner(), 21342, 1, current)->useSkill();
			if (getOwner().getController().useSkill(21342)) {
				skillTask.set(ThreadPoolManager::getInstance().schedule({this}, [this] { AIActions::deleteOwner(*this); }, 3000));
			}
		}
	}
}

// Java EnragedNightmareAI.java:96-99
void EnragedNightmareAI::handleBackHome() {
}

} // namespace aion::gameserver::handlers::ai::instance::nightmareCircus
