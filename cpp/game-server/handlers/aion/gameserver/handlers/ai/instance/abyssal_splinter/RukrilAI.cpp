#include "aion/gameserver/handlers/ai/instance/abyssal_splinter/RukrilAI.h"

#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::abyssal_splinter {

AION_AI(RukrilAI, "rukril");

// Java RukrilAI.java:28-32
void RukrilAI::handleAttack(runtime::Ptr<Creature> creature) {
	AggressiveNpcAI::handleAttack(creature);
	hpPhases->tryEnterNextPhase(*this);
}

// Java RukrilAI.java:34-37
void RukrilAI::handleHpPhase(int32_t phaseHpPercent) {
	startSkillTask();
}

// Java RukrilAI.java:39-51
void RukrilAI::startSkillTask() {
	skillTask.set(ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] {
		if (isDead())
			cancelTask();
		else {
			SkillEngine::getInstance().getSkill(getOwner(), 19266, 55, runtime::Ptr<VisibleObject>(getOwner()))->useNoAnimationSkill();
			if (getPosition()->getWorldMapInstance()->getNpc(281907) == nullptr) {
				spawn(281907, 447.3828f, 675.9968f, 433.95636f, int8_t{19});
				spawn(281907, 441.49512f, 680.38495f, 434.02753f, int8_t{19});
			}
		}
	}, 5000, 70000));
}

// Java RukrilAI.java:53-57
void RukrilAI::cancelTask() {
	runtime::FutureRef task = skillTask.get();
	if (task != nullptr && !task->isCancelled()) {
		task->cancel(true);
	}
}

// Java RukrilAI.java:59-63
void RukrilAI::handleDied() {
	AggressiveNpcAI::handleDied();
	cancelTask();
}

// Java RukrilAI.java:65-71
void RukrilAI::handleBackHome() {
	AggressiveNpcAI::handleBackHome();
	cancelTask();
	hpPhases->reset();
	getEffectController()->removeEffect(19266);
}

// Java RukrilAI.java:73-77
void RukrilAI::handleDespawned() {
	cancelTask();
	AggressiveNpcAI::handleDespawned();
}

// Java RukrilAI.java:79-85
bool RukrilAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::REWARD_LOOT:
		case AIQuestion::REWARD_AP:
			return false;
		default:
			return AggressiveNpcAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::abyssal_splinter
