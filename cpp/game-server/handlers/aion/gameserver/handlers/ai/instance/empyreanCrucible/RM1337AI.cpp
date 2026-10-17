#include "aion/gameserver/handlers/ai/instance/empyreanCrucible/RM1337AI.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"

namespace aion::gameserver::handlers::ai::instance::empyreanCrucible {

AION_AI(RM1337AI, "rm_1337");

// Java RM1337AI.java:31-35
void RM1337AI::handleSpawned() {
	AggressiveNpcAI::handleSpawned();
	PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(getOwner()), 1500229, 2000);
}

// Java RM1337AI.java:37-41
void RM1337AI::handleDespawned() {
	cancelTask();
	AggressiveNpcAI::handleDespawned();
}

// Java RM1337AI.java:43-48
void RM1337AI::handleDied() {
	cancelTask();
	PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(getOwner()), 1500231);
	AggressiveNpcAI::handleDied();
}

// Java RM1337AI.java:50-54
void RM1337AI::handleBackHome() {
	cancelTask();
	AggressiveNpcAI::handleBackHome();
}

// Java RM1337AI.java:56-67
void RM1337AI::handleAttack(runtime::Ptr<Creature> creature) {
	AggressiveNpcAI::handleAttack(creature);
	if (isHome.compareAndSet(true, false)) {
		startSkillTask1();
	}
	if (getLifeStats()->getHpPercentage() <= 75) {
		if (isEventStarted.compareAndSet(false, true)) {
			startSkillTask2();
		}
	}
}

// Java RM1337AI.java:69-76
void RM1337AI::cancelTask() {
	runtime::FutureRef first = task1.get();
	if (first != nullptr && !first->isCancelled()) {
		first->cancel(true);
	}
	runtime::FutureRef second = task2.get();
	if (second != nullptr && !second->isCancelled()) {
		second->cancel(true);
	}
}

// Java RM1337AI.java:78-105 (the anonymous Runnable is a lambda; the inner one captures the target)
void RM1337AI::startSkillTask1() {
	task1 = ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] {
		if (isDead()) {
			cancelTask();
		} else {
			if (getOwner().getCastingSkill() != nullptr)
				return;
			if (getLifeStats()->getHpPercentage() <= 50) {
				if (commons::utils::Rnd::nextBoolean()) {
					skillengine::SkillEngine::getInstance().getSkill(getOwner(), 19550, 10, runtime::Ptr<VisibleObject>(getRandomTarget()))->useNoAnimationSkill();
				} else {
					runtime::Ptr<Creature> target = getRandomTarget();
					skillengine::SkillEngine::getInstance().getSkill(getOwner(), 19552, 10, runtime::Ptr<VisibleObject>(target))->useNoAnimationSkill();
					ThreadPoolManager::getInstance().schedule({this}, [this, target = runtime::Ref<Creature>(target)] {
						if (!isDead()) {
							skillengine::SkillEngine::getInstance().getSkill(getOwner(), 19553, 10, runtime::Ptr<VisibleObject>(target))->useNoAnimationSkill();
						}
					}, 4000);
				}
			} else
				skillengine::SkillEngine::getInstance().getSkill(getOwner(), 19550, 10, runtime::Ptr<VisibleObject>(getRandomTarget()))->useNoAnimationSkill();
		}
	}, 10000, 23000);
}

// Java RM1337AI.java:107-122
void RM1337AI::startSkillTask2() {
	task2 = ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] {
		if (isDead()) {
			cancelTask();
		} else {
			getOwner().getController().cancelCurrentSkill(nullptr);
			PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(getOwner()), 1500230);
			skillengine::SkillEngine::getInstance().getSkill(getOwner(), 19551, 10, getTarget())->useNoAnimationSkill();
			spawnSparks();
		}
	}, 0, 60000);
}

// Java RM1337AI.java:124-137
void RM1337AI::spawnSparks() {
	ThreadPoolManager::getInstance().schedule({this}, [this] {
		if (!isDead()) {
			int32_t count = commons::utils::Rnd::get(8, 12);
			for (int32_t i = 0; i < count; i++) {
				rndSpawnInRange(282373, 3, 12);
			}
		}
	}, 4000);
}

// Java RM1337AI.java:139-141
runtime::Ptr<Creature> RM1337AI::getRandomTarget() {
	return getAggroList().getTarget(AggroTarget::RANDOM, 37);
}

} // namespace aion::gameserver::handlers::ai::instance::empyreanCrucible
