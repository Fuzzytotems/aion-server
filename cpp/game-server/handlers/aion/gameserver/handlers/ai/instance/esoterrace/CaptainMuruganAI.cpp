#include "aion/gameserver/handlers/ai/instance/esoterrace/CaptainMuruganAI.h"

#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"

namespace aion::gameserver::handlers::ai::instance::esoterrace {

AION_AI(CaptainMuruganAI, "captain_murugan");

// Java CaptainMuruganAI.java:32-37
void CaptainMuruganAI::handleAttack(runtime::Ptr<Creature> creature) {
	AggressiveNpcAI::handleAttack(creature);
	if (isAggred.compareAndSet(false, true)) {
		startTaskEvent();
	}
}

/** Java's `VisibleObject target = getTarget(); if (target instanceof Player) getSkill(getOwner(), 19324, 10, target).useNoAnimationSkill();` */
void CaptainMuruganAI::castOnPlayerTarget() {
	runtime::Ptr<VisibleObject> target = getTarget();
	if (runtime::as<Player>(target) != nullptr) {
		skillengine::SkillEngine::getInstance().getSkill(getOwner(), 19324, 10, target)->useNoAnimationSkill();
	}
}

// Java CaptainMuruganAI.java:39-89
void CaptainMuruganAI::startTaskEvent() {
	castOnPlayerTarget();
	task.set(ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] { runTask(); }, 20000, 20000));
}

// Java CaptainMuruganAI.java:47-86 (the inner Runnables)
void CaptainMuruganAI::runTask() {
	if (isDead()) {
		cancelTask();
	} else {
		PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(getOwner()), 1500194);
		skillengine::SkillEngine::getInstance().getSkill(getOwner(), 19325, 5, runtime::Ptr<VisibleObject>(getOwner()))->useNoAnimationSkill();
		if (getLifeStats()->getHpPercentage() < 50) {
			specialSkillTask.set(ThreadPoolManager::getInstance().schedule({this}, [this] {
				if (!isDead()) {
					PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(getOwner()), 1500193);
					castOnPlayerTarget();
					specialSkillTask.set(ThreadPoolManager::getInstance().schedule({this}, [this] {
						if (!isDead()) {
							castOnPlayerTarget();
						}
					}, 4000));
				}
			}, 10000));
		}
	}
}

// Java CaptainMuruganAI.java:91-95
void CaptainMuruganAI::cancelTask() {
	runtime::FutureRef current = task.get();
	if (current != nullptr && !current->isDone()) {
		current->cancel(true);
	}
}

// Java CaptainMuruganAI.java:97-101
void CaptainMuruganAI::cancelSpecialSkillTask() {
	runtime::FutureRef current = specialSkillTask.get();
	if (current != nullptr && !current->isDone()) {
		current->cancel(true);
	}
}

// Java CaptainMuruganAI.java:104-109
void CaptainMuruganAI::handleBackHome() {
	cancelTask();
	cancelSpecialSkillTask();
	AggressiveNpcAI::handleBackHome();
	isAggred.set(false);
}

// Java CaptainMuruganAI.java:112-117
void CaptainMuruganAI::handleDespawned() {
	cancelTask();
	cancelSpecialSkillTask();
	AggressiveNpcAI::handleDespawned();
	isAggred.set(false);
}

// Java CaptainMuruganAI.java:120-126
void CaptainMuruganAI::handleDied() {
	cancelTask();
	cancelSpecialSkillTask();
	PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(getOwner()), 1500195);
	AggressiveNpcAI::handleDied();
	isAggred.set(false);
}

} // namespace aion::gameserver::handlers::ai::instance::esoterrace
