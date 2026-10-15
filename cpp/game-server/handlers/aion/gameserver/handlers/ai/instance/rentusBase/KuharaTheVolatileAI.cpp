#include "aion/gameserver/handlers/ai/instance/rentusBase/KuharaTheVolatileAI.h"

#include <cmath>
#include <numbers>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/ai/manager/EmoteManager.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/controllers/movement/NpcMoveController.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::rentusBase {

AION_AI(KuharaTheVolatileAI, "kuhara_the_volatile");

// Java KuharaTheVolatileAI.java:37-40
bool KuharaTheVolatileAI::canThink() {
	return canThink_.get();
}

// Java KuharaTheVolatileAI.java:42-49
void KuharaTheVolatileAI::handleAttack(runtime::Ptr<Creature> creature) {
	AggressiveNpcAI::handleAttack(creature);
	if (isStarted.compareAndSet(false, true)) {
		startActiveEvent();
		startBarrelEvent();
	}
}

// Java KuharaTheVolatileAI.java:51-54
void KuharaTheVolatileAI::cancelTask(runtime::FutureRef task) {
	if (task != nullptr && !task->isCancelled())
		task->cancel(true);
}

// Java KuharaTheVolatileAI.java:56-78
void KuharaTheVolatileAI::startBarrelEvent() {
	barrelEventTask.set(ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] {
		switch (commons::utils::Rnd::get(1, 4)) {
			case 1:
				rndSpawn(282394, 126.53f, 274.49f, 209.819f);
				rndSpawn(282394, 126.53f, 274.49f, 209.819f);
				break;
			case 2:
				rndSpawn(282394, 162.22f, 263.89f, 209.819f);
				rndSpawn(282394, 162.22f, 263.89f, 209.819f);
				break;
			case 3:
				rndSpawn(282394, 156.32f, 235.73f, 209.819f);
				rndSpawn(282394, 156.32f, 235.73f, 209.819f);
				break;
			case 4:
				rndSpawn(282394, 119.24f, 245.89f, 209.819f);
				rndSpawn(282394, 119.24f, 245.89f, 209.819f);
				break;
			default:
				break;
		}
		startBombEvent();
	}, 50000, 50000));
}

// Java KuharaTheVolatileAI.java:80-113
void KuharaTheVolatileAI::startBombEvent() {
	bombEventTask.set(ThreadPoolManager::getInstance().schedule({this}, [this] {
		if (!isDead()) {
			PacketSendUtility::broadcastMessage(getOwner(), 1500394);
			canThink_ = false;
			EmoteManager::emoteStopAttacking(getOwner());
			setStateIfNot(AIState::WALKING);
			SkillEngine::getInstance().getSkill(getOwner(), 19703, 60, runtime::Ptr<VisibleObject>(getOwner()))->useNoAnimationSkill();
			spawnBombEvent();

			bombEventTask.set(ThreadPoolManager::getInstance().schedule({this}, [this] {
				if (!isDead()) {
					canThink_ = true;
					runtime::Ptr<Creature> creature = getAggroList().getTarget(AggroTarget::MOST_HATED);
					if (creature == nullptr) {
						setStateIfNot(AIState::FIGHT);
						think();
					} else {
						getMoveController()->abortMove();
						getOwner().setTarget(creature);
						getOwner().getGameStats()->renewLastAttackTime();
						getOwner().getGameStats()->renewLastAttackedTime();
						getOwner().getGameStats()->renewLastSkillTime();
						setStateIfNot(AIState::FIGHT);
						handleMoveValidate();
						SkillEngine::getInstance().getSkill(getOwner(), 19375, 60, runtime::Ptr<VisibleObject>(getOwner()))->useNoAnimationSkill();
					}
					deleteNpcs(getPosition()->getWorldMapInstance()->getNpcs({282394}));
					deleteNpcs(getPosition()->getWorldMapInstance()->getNpcs({282396}));
				}
			}, 11000));
		}
	}, 14000));
}

// Java KuharaTheVolatileAI.java:115-120
void KuharaTheVolatileAI::deleteNpcs(const std::vector<runtime::Ptr<Npc>>& npcs) {
	for (const runtime::Ptr<Npc>& npc : npcs) {
		if (npc != nullptr)
			npc->getController().delete_();
	}
}

// Java KuharaTheVolatileAI.java:122-131
void KuharaTheVolatileAI::spawnBombEvent() {
	moveBombToBoss(rndSpawn(282396, 126.53f, 274.49f, 209.819f));
	moveBombToBoss(rndSpawn(282396, 126.53f, 274.49f, 209.819f));
	moveBombToBoss(rndSpawn(282396, 162.22f, 263.89f, 209.819f));
	moveBombToBoss(rndSpawn(282396, 162.22f, 263.89f, 209.819f));
	moveBombToBoss(rndSpawn(282396, 156.32f, 235.73f, 209.819f));
	moveBombToBoss(rndSpawn(282396, 156.32f, 235.73f, 209.819f));
	moveBombToBoss(rndSpawn(282396, 119.24f, 245.89f, 209.819f));
	moveBombToBoss(rndSpawn(282396, 119.24f, 245.89f, 209.819f));
}

// Java KuharaTheVolatileAI.java:133-138
void KuharaTheVolatileAI::moveBombToBoss(runtime::Ptr<Npc> npc) {
	if (!isDead()) {
		npc->setTarget(runtime::Ptr<VisibleObject>(getOwner()));
		npc->getMoveController()->moveToTargetObject();
	}
}

// Java KuharaTheVolatileAI.java:140-146 (Math.toRadians: angdeg / 180.0 * PI)
runtime::Ptr<Npc> KuharaTheVolatileAI::rndSpawn(int32_t npcId, float x, float y, float z) {
	double angleRadians = static_cast<double>(commons::utils::Rnd::nextFloat(360.0f)) / 180.0 * std::numbers::pi;
	float distance = static_cast<float>(commons::utils::Rnd::get(0, 4));
	float x1 = static_cast<float>(std::cos(angleRadians) * distance) + x;
	float y1 = static_cast<float>(std::sin(angleRadians) * distance) + y;
	return runtime::cast<Npc>(spawn(npcId, x1, y1, z, int8_t{0}));
}

// Java KuharaTheVolatileAI.java:148-161
void KuharaTheVolatileAI::startActiveEvent() {
	activeEventTask.set(ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] {
		PacketSendUtility::broadcastMessage(getOwner(), 1500395);
		ThreadPoolManager::getInstance().schedule({this}, [this] {
			if (!isDead()) {
				SkillEngine::getInstance().getSkill(getOwner(), 19704, 60, runtime::Ptr<VisibleObject>(getOwner()))->useNoAnimationSkill();
				ThreadPoolManager::getInstance().schedule({this}, [this] {
					if (!isDead())
						SkillEngine::getInstance().getSkill(getOwner(), 19705, 60, runtime::Ptr<VisibleObject>(getOwner()))->useNoAnimationSkill();
				}, 3500);
			}
		}, 1000);
	}, 8000, 14000));
}

// Java KuharaTheVolatileAI.java:163-169
void KuharaTheVolatileAI::handleDespawned() {
	AggressiveNpcAI::handleDespawned();
	cancelTask(activeEventTask.get());
	cancelTask(barrelEventTask.get());
	cancelTask(bombEventTask.get());
}

// Java KuharaTheVolatileAI.java:171-179
void KuharaTheVolatileAI::handleBackHome() {
	isStarted.set(false);
	canThink_ = true;
	cancelTask(activeEventTask.get());
	cancelTask(barrelEventTask.get());
	cancelTask(bombEventTask.get());
	AggressiveNpcAI::handleBackHome();
}

// Java KuharaTheVolatileAI.java:181-192
void KuharaTheVolatileAI::handleDied() {
	cancelTask(activeEventTask.get());
	cancelTask(barrelEventTask.get());
	cancelTask(bombEventTask.get());
	const runtime::Ptr<WorldPosition> p = getPosition();
	if (p != nullptr) {
		deleteNpcs(p->getWorldMapInstance()->getNpcs({282394}));
		deleteNpcs(p->getWorldMapInstance()->getNpcs({282396}));
	}
	AggressiveNpcAI::handleDied();
}

} // namespace aion::gameserver::handlers::ai::instance::rentusBase
