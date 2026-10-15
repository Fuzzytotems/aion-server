#include "aion/gameserver/handlers/ai/instance/sauroBase/MoriataAI.h"

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Effect.h" // the Ref<Effect> applyEffectDirectly returns

namespace aion::gameserver::handlers::ai::instance::sauroBase {

AION_AI(MoriataAI, "moriata");

// Java MoriataAI.java:30-35
void MoriataAI::handleAttack(runtime::Ptr<Creature> creature) {
	AggressiveNpcAI::handleAttack(creature);
	if (hasAggro.compareAndSet(false, true))
		task.set(ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] { observeKiting(); }, 12000, 12000));
}

// Java MoriataAI.java:37-44
void MoriataAI::observeKiting() {
	if ((commons::utils::currentTimeMillis() - lastSkillUse.get()) / 1000 >= 10) {
		SkillEngine::getInstance().applyEffectDirectly(20181, getOwner(), getOwner()); // Temporary Speed Buff
		SkillEngine::getInstance().applyEffectDirectly(21417, getOwner(), getOwner()); // Raging Fury
		kiteCount = kiteCount + 1;
		if (kiteCount.get() % 3 == 0)
			SkillEngine::getInstance().applyEffectDirectly(22661, getOwner(), getOwner()); // Permanent Speed Buff
	}
}

// Java MoriataAI.java:46-50
void MoriataAI::handleAttackComplete() {
	lastSkillUse = commons::utils::currentTimeMillis();
	AggressiveNpcAI::handleAttackComplete();
}

// Java MoriataAI.java:52-55
void MoriataAI::onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) {
	lastSkillUse = commons::utils::currentTimeMillis();
}

// Java MoriataAI.java:57-60
void MoriataAI::cancelTask() {
	runtime::FutureRef current = task.get();
	if (current != nullptr && !current->isCancelled())
		current->cancel(true);
}

// Java MoriataAI.java:62-67
void MoriataAI::handleBackHome() {
	cancelTask();
	hasAggro.set(false);
	AggressiveNpcAI::handleBackHome();
}

// Java MoriataAI.java:69-73
void MoriataAI::handleDied() {
	cancelTask();
	AggressiveNpcAI::handleDied();
}

// Java MoriataAI.java:75-79
void MoriataAI::handleDespawned() {
	cancelTask();
	AggressiveNpcAI::handleDespawned();
}

} // namespace aion::gameserver::handlers::ai::instance::sauroBase
