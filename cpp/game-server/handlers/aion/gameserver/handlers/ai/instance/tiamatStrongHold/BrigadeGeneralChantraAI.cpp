#include "aion/gameserver/handlers/ai/instance/tiamatStrongHold/BrigadeGeneralChantraAI.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold {

AION_AI(BrigadeGeneralChantraAI, "brigadegeneralchantra");

// Java BrigadeGeneralChantraAI.java:29-38
void BrigadeGeneralChantraAI::handleAttack(runtime::Ptr<Creature> creature) {
	AggressiveNpcAI::handleAttack(creature);
	if (isHome.compareAndSet(true, false))
		startSkillTask();
	if (!isFinalBuff.get() && getOwner().getLifeStats()->getHpPercentage() <= 25) {
		isFinalBuff = true;
		AIActions::useSkill(*this, 20942);
	}
}

// Java BrigadeGeneralChantraAI.java:40-52 (the anonymous Runnable: a task pinning this)
void BrigadeGeneralChantraAI::startSkillTask() {
	trapTask.set(ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] {
		if (isDead())
			cancelTask();
		else {
			startTrapEvent();
		}
	}, 5000, 40000));
}

// Java BrigadeGeneralChantraAI.java:54-58
void BrigadeGeneralChantraAI::cancelTask() {
	runtime::FutureRef task = trapTask.get();
	if (task != nullptr && !task->isCancelled()) {
		task->cancel(true);
	}
}

// Java BrigadeGeneralChantraAI.java:60-78 (the anonymous Runnable: a task pinning this)
void BrigadeGeneralChantraAI::startTrapEvent() {
	const int32_t trap = commons::utils::Rnd::get({283092, 283094}); // 4.0
	if (getPosition()->getWorldMapInstance()->getNpc(trap) == nullptr) {
		spawn(trap, 1031.1f, 466.38f, 445.45f, int8_t{0});
		ThreadPoolManager::getInstance().schedule({this}, [this, trap] {
			runtime::Ptr<Npc> ring = getPosition()->getWorldMapInstance()->getNpc(trap);
			if (trap == 283092) // 4.0
				spawn(283171, 1031.1f, 466.38f, 445.45f, int8_t{0}); // 4.0
			else
				spawn(283172, 1031.1f, 466.38f, 445.45f, int8_t{0}); // 4.0
			ring->getController().delete_();
		}, 5000);
	}
}

// Java BrigadeGeneralChantraAI.java:80-84
void BrigadeGeneralChantraAI::handleDied() {
	AggressiveNpcAI::handleDied();
	cancelTask();
}

// Java BrigadeGeneralChantraAI.java:86-90
void BrigadeGeneralChantraAI::handleDespawned() {
	AggressiveNpcAI::handleDespawned();
	cancelTask();
}

// Java BrigadeGeneralChantraAI.java:92-98
void BrigadeGeneralChantraAI::handleBackHome() {
	AggressiveNpcAI::handleBackHome();
	cancelTask();
	isFinalBuff = false;
	isHome.set(true);
}

} // namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold
