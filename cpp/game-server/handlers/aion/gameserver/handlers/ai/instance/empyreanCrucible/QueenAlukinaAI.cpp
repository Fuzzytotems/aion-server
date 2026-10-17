#include "aion/gameserver/handlers/ai/instance/empyreanCrucible/QueenAlukinaAI.h"

#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"

namespace aion::gameserver::handlers::ai::instance::empyreanCrucible {

AION_AI(QueenAlukinaAI, "alukina_emp");

// Java QueenAlukinaAI.java:28-32
void QueenAlukinaAI::handleDespawned() {
	cancelTask();
	AggressiveNpcAI::handleDespawned();
}

// Java QueenAlukinaAI.java:34-38
void QueenAlukinaAI::handleDied() {
	cancelTask();
	AggressiveNpcAI::handleDied();
}

// Java QueenAlukinaAI.java:40-45
void QueenAlukinaAI::handleBackHome() {
	cancelTask();
	AggressiveNpcAI::handleBackHome();
	hpPhases->reset();
}

// Java QueenAlukinaAI.java:47-51
void QueenAlukinaAI::handleAttack(runtime::Ptr<Creature> creature) {
	AggressiveNpcAI::handleAttack(creature);
	hpPhases->tryEnterNextPhase(*this);
}

// Java QueenAlukinaAI.java:53-80
void QueenAlukinaAI::handleHpPhase(int32_t phaseHpPercent) {
	skillengine::SkillEngine::getInstance().getSkill(getOwner(), 17899, 41, getTarget())->useNoAnimationSkill();
	switch (phaseHpPercent) {
		case 75:
			scheduleSkill(17900, 4500);
			PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(getOwner()), 340487, 10000);
			scheduleSkill(17899, 14000);
			scheduleSkill(17900, 18000);
			break;
		case 50:
			scheduleSkill(17280, 4500);
			scheduleSkill(17902, 8000);
			break;
		case 25:
			task = ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] {
				if (isDead()) {
					cancelTask();
				} else {
					skillengine::SkillEngine::getInstance().getSkill(getOwner(), 17901, 41, getTarget())->useNoAnimationSkill();
					scheduleSkill(17902, 5500);
					scheduleSkill(17902, 7500);
				}
			}, 4500, 20000);
			break;
	}
}

// Java QueenAlukinaAI.java:82-85
void QueenAlukinaAI::cancelTask() {
	runtime::FutureRef current = task.get();
	if (current != nullptr && !current->isCancelled())
		current->cancel(true);
}

// Java QueenAlukinaAI.java:87-93
void QueenAlukinaAI::scheduleSkill(int32_t skill, int32_t delay) {
	ThreadPoolManager::getInstance().schedule({this}, [this, skill] {
		if (!isDead()) {
			skillengine::SkillEngine::getInstance().getSkill(getOwner(), skill, 41, getTarget())->useNoAnimationSkill();
		}
	}, delay);
}

} // namespace aion::gameserver::handlers::ai::instance::empyreanCrucible
