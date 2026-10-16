#include "aion/gameserver/handlers/ai/instance/empyreanCrucible/KingConsierdAI.h"

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::empyreanCrucible {

AION_AI(KingConsierdAI, "king_consierd");

// Java KingConsierdAI.java:32-36
void KingConsierdAI::handleDespawned() {
	cancelTasks();
	AggressiveNpcAI::handleDespawned();
}

// Java KingConsierdAI.java:38-43
void KingConsierdAI::handleDied() {
	cancelTasks();
	despawnNpcs(getPosition()->getWorldMapInstance()->getNpcs({282378}));
	AggressiveNpcAI::handleDied();
}

// Java KingConsierdAI.java:45-51
void KingConsierdAI::handleBackHome() {
	cancelTasks();
	despawnNpcs(getPosition()->getWorldMapInstance()->getNpcs({282378}));
	AggressiveNpcAI::handleBackHome();
	hpPhases->reset();
}

// Java KingConsierdAI.java:53-77 (the anonymous Runnables are lambdas)
void KingConsierdAI::handleAttack(runtime::Ptr<Creature> creature) {
	AggressiveNpcAI::handleAttack(creature);
	hpPhases->tryEnterNextPhase(*this);
	if (isHome.compareAndSet(true, false)) {
		startBloodThirstTask();
		ThreadPoolManager::getInstance().schedule({this}, [this] {
			skillengine::SkillEngine::getInstance().getSkill(getOwner(), 19691, 1, getTarget())->useNoAnimationSkill();
			ThreadPoolManager::getInstance().schedule({this}, [this] {
				skillengine::SkillEngine::getInstance().getSkill(getOwner(), 17954, 29, getTarget())->useNoAnimationSkill();
			}, 4000);
		}, 2000);
	}
}

// Java KingConsierdAI.java:79-85
void KingConsierdAI::handleHpPhase(int32_t phaseHpPercent) {
	switch (phaseHpPercent) {
		case 75:
			startSkillTask();
			break;
		case 25:
			skillengine::SkillEngine::getInstance().getSkill(getOwner(), 19690, 1, getTarget())->useNoAnimationSkill();
			break;
	}
}

// Java KingConsierdAI.java:87-96
void KingConsierdAI::startBloodThirstTask() {
	eventTask = ThreadPoolManager::getInstance().schedule({this}, [this] {
		skillengine::SkillEngine::getInstance().getSkill(getOwner(), 19624, 10, runtime::Ptr<VisibleObject>(getOwner()))->useNoAnimationSkill();
	}, 180 * 1000); // 3min, need confirm
}

// Java KingConsierdAI.java:98-114
void KingConsierdAI::startSkillTask() {
	skillTask = ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] {
		if (isDead()) {
			cancelTasks();
			return;
		}
		skillengine::SkillEngine::getInstance().getSkill(getOwner(), 17951, 29, getTarget())->useNoAnimationSkill();
		ThreadPoolManager::getInstance().schedule({this}, [this] {
			if (getLifeStats()->getHpPercentage() <= 50) {
				runtime::Ptr<world::WorldPosition> p = getPosition();
				spawn(282378, p->getX(), p->getY(), p->getZ(), p->getHeading());
				spawn(282378, p->getX(), p->getY(), p->getZ(), p->getHeading());
			}
			ThreadPoolManager::getInstance().schedule({this}, [this] { skillengine::SkillEngine::getInstance().getSkill(getOwner(), 17952, 29, getTarget())->useNoAnimationSkill(); }, 2000);
		}, 3500);
	}, 0, 25000);
}

// Java KingConsierdAI.java:116-123
void KingConsierdAI::cancelTasks() {
	runtime::FutureRef event = eventTask.get();
	if (event != nullptr && !event->isDone()) {
		event->cancel(true);
	}
	runtime::FutureRef skills = skillTask.get();
	if (skills != nullptr && !skills->isCancelled()) {
		skills->cancel(true);
	}
}

// Java KingConsierdAI.java:125-129
void KingConsierdAI::despawnNpcs(const std::vector<runtime::Ptr<Npc>>& npcs) {
	for (const runtime::Ptr<Npc>& npc : npcs) {
		npc->getController().delete_();
	}
}

} // namespace aion::gameserver::handlers::ai::instance::empyreanCrucible
