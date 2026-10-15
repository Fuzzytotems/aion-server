#include "aion/gameserver/handlers/ai/instance/tallocsHollow/KinquidAI.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::tallocsHollow {

AION_AI(KinquidAI, "kinquid");

// Java KinquidAI.java:29-39
void KinquidAI::handleCreatureAggro(Creature& creature) {
	AggressiveNpcAI::handleCreatureAggro(creature);
	if (isHome.compareAndSet(true, false)) {
		getPosition()->getWorldMapInstance()->setDoorState(48, false);
		cancelSkillTask();
		skillTask.set(ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] { runSkillTask(); }, 35000, 35000));
		cancelDestroyerRespawnTask();
		destroyerRespawnTask.set(ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] { respawnDestroyer(); }, 0, 25000));
	}
}

// Java KinquidAI.java:41-49
void KinquidAI::handleBackHome() {
	cancelSkillTask();
	cancelDestroyerRespawnTask();
	isHome.set(true);
	getPosition()->getWorldMapInstance()->setDoorState(48, true);
	AggressiveNpcAI::handleBackHome();
	despawnDestroyer();
}

// Java KinquidAI.java:51-56
void KinquidAI::handleDespawned() {
	cancelSkillTask();
	cancelDestroyerRespawnTask();
	AggressiveNpcAI::handleDespawned();
}

// Java KinquidAI.java:58-63
void KinquidAI::handleDied() {
	cancelSkillTask();
	cancelDestroyerRespawnTask();
	AggressiveNpcAI::handleDied();
}

// Java KinquidAI.java:65-70
void KinquidAI::cancelSkillTask() {
	runtime::FutureRef task = skillTask.get();
	if (task != nullptr && !task->isDone()) {
		task->cancel(true);
		skillTask.set(nullptr);
	}
}

// Java KinquidAI.java:72-77
void KinquidAI::cancelDestroyerRespawnTask() {
	runtime::FutureRef task = destroyerRespawnTask.get();
	if (task != nullptr && !task->isDone()) {
		task->cancel(true);
		destroyerRespawnTask.set(nullptr);
	}
}

// Java KinquidAI.java:79-89
void KinquidAI::runSkillTask() {
	if (isDead() || !getPosition()->isSpawned()) {
		cancelSkillTask();
		return;
	}
	SkillEngine::getInstance().getSkill(getOwner(), 19233, 60, runtime::Ptr<VisibleObject>(getOwner()))->useNoAnimationSkill();
	ThreadPoolManager::getInstance().schedule({this}, [this] {
		if (!isDead() && getPosition()->isSpawned())
			SkillEngine::getInstance().getSkill(getOwner(), 19234, 60, runtime::Ptr<VisibleObject>(getOwner()))->useNoAnimationSkill();
	}, 3500);
}

// Java KinquidAI.java:91-98
void KinquidAI::despawnDestroyer() {
	runtime::Ptr<Npc> cleaveArmor = getPosition()->getWorldMapInstance()->getNpc(282008);
	if (cleaveArmor != nullptr)
		cleaveArmor->getController().delete_();
	runtime::Ptr<Npc> accessoryDestruction = getPosition()->getWorldMapInstance()->getNpc(282009);
	if (accessoryDestruction != nullptr)
		accessoryDestruction->getController().delete_();
}

// Java KinquidAI.java:100-110
void KinquidAI::respawnDestroyer() {
	despawnDestroyer();
	if (getPosition()->isSpawned() && !isDead() && !isHome.get()) {
		int32_t spawnId = commons::utils::Rnd::nextBoolean() ? 282008 : 282009;
		switch (commons::utils::Rnd::get(1, 3)) {
			case 1:
				spawn(spawnId, 266.70685f, 680.6733f, 1167.2369f, int8_t{0});
				break;
			case 2:
				spawn(spawnId, 292.02466f, 719.7132f, 1169.3982f, int8_t{0});
				break;
			case 3:
				spawn(spawnId, 263.4334f, 716.73004f, 1170.3693f, int8_t{0});
				break;
			default:
				break;
		}
	}
}

} // namespace aion::gameserver::handlers::ai::instance::tallocsHollow
