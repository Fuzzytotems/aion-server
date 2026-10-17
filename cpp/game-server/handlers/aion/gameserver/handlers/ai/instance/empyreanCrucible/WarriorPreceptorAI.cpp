#include "aion/gameserver/handlers/ai/instance/empyreanCrucible/WarriorPreceptorAI.h"

#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"

namespace aion::gameserver::handlers::ai::instance::empyreanCrucible {

AION_AI(WarriorPreceptorAI, "warrior_preceptor");

// Java WarriorPreceptorAI.java:29-33
void WarriorPreceptorAI::handleDespawned() {
	cancelTask();
	AggressiveNpcAI::handleDespawned();
}

// Java WarriorPreceptorAI.java:35-40
void WarriorPreceptorAI::handleDied() {
	cancelTask();
	PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(getOwner()), 1500208);
	AggressiveNpcAI::handleDied();
}

// Java WarriorPreceptorAI.java:42-47
void WarriorPreceptorAI::handleBackHome() {
	cancelTask();
	isHome.set(true);
	AggressiveNpcAI::handleBackHome();
}

// Java WarriorPreceptorAI.java:49-56
void WarriorPreceptorAI::handleAttack(runtime::Ptr<Creature> creature) {
	AggressiveNpcAI::handleAttack(creature);
	if (isHome.compareAndSet(true, false)) {
		startSkillTask();
	}
}

// Java WarriorPreceptorAI.java:58-71 (the anonymous Runnable is a lambda)
void WarriorPreceptorAI::startSkillTask() {
	task = ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] {
		if (isDead()) {
			cancelTask();
		} else {
			startSkillEvent();
		}
	}, 30000, 30000);
}

// Java WarriorPreceptorAI.java:73-77
void WarriorPreceptorAI::cancelTask() {
	runtime::FutureRef current = task.get();
	if (current != nullptr && !current->isCancelled()) {
		current->cancel(true);
	}
}

// Java WarriorPreceptorAI.java:79-87
void WarriorPreceptorAI::startSkillEvent() {
	PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(getOwner()), 1500207);
	skillengine::SkillEngine::getInstance().getSkill(getOwner(), 19595, 10, runtime::Ptr<VisibleObject>(getRandomTarget()))->useNoAnimationSkill(); // Divine Grasp II
	ThreadPoolManager::getInstance().schedule({this}, [this] {
		if (!isDead()) {
			skillengine::SkillEngine::getInstance().getSkill(getOwner(), 19596, 15, runtime::Ptr<VisibleObject>(getOwner()))->useNoAnimationSkill(); // Wrathful Wave II
		}
	}, 6000);
}

// Java WarriorPreceptorAI.java:89-91
runtime::Ptr<Creature> WarriorPreceptorAI::getRandomTarget() {
	return getAggroList().getTarget(AggroTarget::RANDOM, 15);
}

} // namespace aion::gameserver::handlers::ai::instance::empyreanCrucible
