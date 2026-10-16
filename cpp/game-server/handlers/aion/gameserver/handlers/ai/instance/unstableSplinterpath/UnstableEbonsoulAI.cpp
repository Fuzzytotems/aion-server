#include "aion/gameserver/handlers/ai/instance/unstableSplinterpath/UnstableEbonsoulAI.h"

#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::unstableSplinterpath {

AION_AI(UnstableEbonsoulAI, "unstableebonsoul");

// Java UnstableEbonsoulAI.java:30-35
void UnstableEbonsoulAI::handleAttack(runtime::Ptr<Creature> creature) {
	AggressiveNpcAI::handleAttack(creature);
	hpPhases->tryEnterNextPhase(*this);
	tryRegen();
}

// Java UnstableEbonsoulAI.java:37-40
void UnstableEbonsoulAI::handleHpPhase(int32_t phaseHpPercent) {
	startSkillTask();
}

// Java UnstableEbonsoulAI.java:42-58 (the rukril looked up once, kept alive by the task, as Java's capture does)
void UnstableEbonsoulAI::startSkillTask() {
	const runtime::Ref<Npc> rukril = getPosition()->getWorldMapInstance()->getNpc(219551);
	skillTask.set(ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this, rukril] {
		if (isDead())
			cancelTask();
		else {
			if (getPosition()->getWorldMapInstance()->getNpc(283205) == nullptr) {
				SkillEngine::getInstance().getSkill(getOwner(), 19159, 55, runtime::Ptr<VisibleObject>(getOwner()))->useNoAnimationSkill();
				spawn(283205, getOwner().getX() + 2, getOwner().getY() - 2, getOwner().getZ(), int8_t{0});
			}
			if (rukril != nullptr && !rukril->isDead()) {
				SkillEngine::getInstance().getSkill(*rukril, 19266, 55, rukril)->useNoAnimationSkill();
				spawn(283204, rukril->getX() + 2, rukril->getY() - 2, rukril->getZ(), int8_t{0});
			}
		}
	}, 5000, 70000)); // re-check delay
}

// Java UnstableEbonsoulAI.java:60-64
void UnstableEbonsoulAI::cancelTask() {
	runtime::FutureRef task = skillTask.get();
	if (task != nullptr && !task->isCancelled()) {
		task->cancel(true);
	}
}

// Java UnstableEbonsoulAI.java:66-72
void UnstableEbonsoulAI::tryRegen() {
	runtime::Ptr<Npc> rukril = getPosition()->getWorldMapInstance()->getNpc(219551);
	if (rukril != nullptr && !rukril->isDead() && PositionUtil::isInRange(getOwner(), *rukril, 5))
		if (!getOwner().getLifeStats()->isFullyRestoredHp())
			getOwner().getLifeStats()->increaseHp(SM_ATTACK_STATUS::TYPE::HP, 10000);
}

// Java UnstableEbonsoulAI.java:74-78
void UnstableEbonsoulAI::handleDied() {
	AggressiveNpcAI::handleDied();
	cancelTask();
}

// Java UnstableEbonsoulAI.java:80-86
void UnstableEbonsoulAI::handleBackHome() {
	AggressiveNpcAI::handleBackHome();
	cancelTask();
	hpPhases->reset();
	getEffectController()->removeEffect(19266);
}

// Java UnstableEbonsoulAI.java:88-92
void UnstableEbonsoulAI::handleDespawned() {
	cancelTask();
	AggressiveNpcAI::handleDespawned();
}

// Java UnstableEbonsoulAI.java:94-100
bool UnstableEbonsoulAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::REWARD_LOOT:
		case AIQuestion::REWARD_AP:
			return false;
		default:
			return AggressiveNpcAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::unstableSplinterpath
