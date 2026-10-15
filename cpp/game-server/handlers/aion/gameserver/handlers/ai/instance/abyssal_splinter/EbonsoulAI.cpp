#include "aion/gameserver/handlers/ai/instance/abyssal_splinter/EbonsoulAI.h"

#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::abyssal_splinter {

AION_AI(EbonsoulAI, "ebonsoul");

// Java EbonsoulAI.java:28-32
void EbonsoulAI::handleAttack(runtime::Ptr<Creature> creature) {
	AggressiveNpcAI::handleAttack(creature);
	hpPhases->tryEnterNextPhase(*this);
}

// Java EbonsoulAI.java:34-37
void EbonsoulAI::handleHpPhase(int32_t phaseHpPercent) {
	startSkillTask();
}

// Java EbonsoulAI.java:39-51
void EbonsoulAI::startSkillTask() {
	skillTask.set(ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] {
		if (isDead())
			cancelTask();
		else {
			SkillEngine::getInstance().getSkill(getOwner(), 19159, 55, runtime::Ptr<VisibleObject>(getOwner()))->useNoAnimationSkill();
			if (getPosition()->getWorldMapInstance()->getNpc(281908) == nullptr) {
				spawn(281908, 462.47913f, 707.4807f, 433.78372f, int8_t{93});
				spawn(281908, 456.09427f, 707.4807f, 433.78372f, int8_t{93});
			}
		}
	}, 5000, 70000)); // re-check delay
}

// Java EbonsoulAI.java:53-57
void EbonsoulAI::cancelTask() {
	runtime::FutureRef task = skillTask.get();
	if (task != nullptr && !task->isCancelled()) {
		task->cancel(true);
	}
}

// Java EbonsoulAI.java:59-63
void EbonsoulAI::handleDied() {
	AggressiveNpcAI::handleDied();
	cancelTask();
}

// Java EbonsoulAI.java:65-71
void EbonsoulAI::handleBackHome() {
	AggressiveNpcAI::handleBackHome();
	cancelTask();
	hpPhases->reset();
	getEffectController()->removeEffect(19266);
}

// Java EbonsoulAI.java:73-77
void EbonsoulAI::handleDespawned() {
	cancelTask();
	AggressiveNpcAI::handleDespawned();
}

// Java EbonsoulAI.java:79-85
bool EbonsoulAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::REWARD_LOOT:
		case AIQuestion::REWARD_AP:
			return false;
		default:
			return AggressiveNpcAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::abyssal_splinter
