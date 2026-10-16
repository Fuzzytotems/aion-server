#include "aion/gameserver/handlers/ai/instance/aturamSkyFortress/PopuchinAI.h"

#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::aturamSkyFortress {

AION_AI(PopuchinAI, "popuchin");

// Java PopuchinAI.java:30-76
void PopuchinAI::startBombTask() {
	if (!isDead() && !isHome.get()) {
		bombTask.set(ThreadPoolManager::getInstance().schedule({this}, [this] {
			if (!isDead() && !isHome.get()) {
				runtime::Ptr<VisibleObject> target = getTarget();
				if (runtime::as<Player>(target) != nullptr) {
					SkillEngine::getInstance().getSkill(getOwner(), 19413, 49, target)->useNoAnimationSkill();
				}
				ThreadPoolManager::getInstance().schedule({this}, [this] {
					if (!isDead() && !isHome.get()) {
						SkillEngine::getInstance().getSkill(getOwner(), 19412, 49, runtime::Ptr<VisibleObject>(getOwner()))->useNoAnimationSkill();
						ThreadPoolManager::getInstance().schedule({this}, [this] {
							if (!isDead() && !isHome.get() && getOwner().isSpawned()) {
								if (getLifeStats()->getHpPercentage() > 50) {
									runtime::Ptr<WorldPosition> p = getPosition();
									if (p != nullptr && p->getWorldMapInstance() != nullptr) {
										spawn(217374, p->getX(), p->getY(), p->getZ(), p->getHeading());
										spawn(217374, p->getX(), p->getY(), p->getZ(), p->getHeading());
										startBombTask();
									}
								} else {
									spawnRndBombs();
									startBombTask();
								}
							}
						}, 1500);
					}
				}, 3000);
			}
		}, 15500));
	}
}

// Java PopuchinAI.java:78-86
void PopuchinAI::handleAttack(runtime::Ptr<Creature> creature) {
	AggressiveNpcAI::handleAttack(creature);
	if (isHome.get()) {
		isHome = false;
		getPosition()->getWorldMapInstance()->setDoorState(68, false);
		startBombTask();
	}
}

// Java PopuchinAI.java:88-96
void PopuchinAI::handleBackHome() {
	isHome = true;
	AggressiveNpcAI::handleBackHome();
	getPosition()->getWorldMapInstance()->setDoorState(68, true);
	if (runtime::FutureRef task = bombTask.get(); task != nullptr && !task->isDone()) {
		task->cancel(true);
	}
}

// Java PopuchinAI.java:98-112
void PopuchinAI::spawnRndBombs() {
	ThreadPoolManager::getInstance().schedule({this}, [this] {
		if (!isDead() && !isHome.get()) {
			for (int32_t i = 0; i < 10; i++) {
				rndSpawnInRange(217375, 1, 12);
			}
		}
	}, 1500);
}

// Java PopuchinAI.java:114-120
bool PopuchinAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::IS_IMMUNE_TO_ABNORMAL_STATES:
			return true;
		default:
			return AggressiveNpcAI::ask(question);
	}
}

// Java PopuchinAI.java:122-126
void PopuchinAI::handleDied() {
	AggressiveNpcAI::handleDied();
	getPosition()->getWorldMapInstance()->setDoorState(68, true);
}

} // namespace aion::gameserver::handlers::ai::instance::aturamSkyFortress
