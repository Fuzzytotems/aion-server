#include "aion/gameserver/handlers/ai/instance/rentusBase/ExplosiveDranaCrystalAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::rentusBase {

AION_AI(ExplosiveDranaCrystalAI, "explosive_drana_crystal");

// Java ExplosiveDranaCrystalAI.java:30-34
void ExplosiveDranaCrystalAI::handleSpawned() {
	ActionItemNpcAI::handleSpawned();
	startLifeTask();
}

// Java ExplosiveDranaCrystalAI.java:36-58
void ExplosiveDranaCrystalAI::handleUseItemFinish(Player& player) {
	if (isUsed.compareAndSet(false, true)) {
		runtime::Ptr<WorldPosition> p = getPosition();
		runtime::Ptr<Npc> boss = p->getWorldMapInstance()->getNpc(217308);
		if (boss != nullptr && !boss->isDead()) {
			runtime::Ptr<controllers::effect::EffectController> ef = boss->getEffectController();
			if (ef->hasAbnormalEffect(19370)) {
				ef->removeEffect(19370);
			} else if (ef->hasAbnormalEffect(19371)) {
				ef->removeEffect(19371);
			} else if (ef->hasAbnormalEffect(19372)) {
				ef->removeEffect(19372);
			}
		}
		runtime::Ptr<Npc> npc = runtime::cast<Npc>(spawn(282530, p->getX(), p->getY(), p->getZ(), p->getHeading()));
		runtime::Ptr<Npc> invisibleNpc = runtime::cast<Npc>(spawn(282529, p->getX(), p->getY(), p->getZ(), p->getHeading()));
		SkillEngine::getInstance().getSkill(*npc, 19373, 60, npc)->useNoAnimationSkill();
		SkillEngine::getInstance().getSkill(*invisibleNpc, 19654, 60, invisibleNpc)->useNoAnimationSkill();
		invisibleNpc->getController().delete_();
		AIActions::deleteOwner(*this);
	}
}

// Java ExplosiveDranaCrystalAI.java:60-64
void ExplosiveDranaCrystalAI::cancelLifeTask() {
	runtime::FutureRef task = lifeTask.get();
	if (task != nullptr && !task->isDone()) {
		task->cancel(true);
	}
}

// Java ExplosiveDranaCrystalAI.java:66-71
void ExplosiveDranaCrystalAI::startLifeTask() {
	lifeTask.set(ThreadPoolManager::getInstance().schedule({this}, [this] {
		if (!isDead())
			AIActions::deleteOwner(*this);
	}, 60000));
}

// Java ExplosiveDranaCrystalAI.java:73-77
void ExplosiveDranaCrystalAI::handleDied() {
	cancelLifeTask();
	ActionItemNpcAI::handleDied();
}

// Java ExplosiveDranaCrystalAI.java:79-83
void ExplosiveDranaCrystalAI::handleDespawned() {
	cancelLifeTask();
	ActionItemNpcAI::handleDespawned();
}

} // namespace aion::gameserver::handlers::ai::instance::rentusBase
