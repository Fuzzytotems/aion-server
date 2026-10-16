#include "aion/gameserver/handlers/ai/instance/unstableSplinterpath/UnstableRukrilAI.h"

#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::unstableSplinterpath {

AION_AI(UnstableRukrilAI, "unstablerukril");

// Java UnstableRukrilAI.java:30-35
void UnstableRukrilAI::handleAttack(runtime::Ptr<Creature> creature) {
	AggressiveNpcAI::handleAttack(creature);
	hpPhases->tryEnterNextPhase(*this);
	tryRegen();
}

// Java UnstableRukrilAI.java:37-40
void UnstableRukrilAI::handleHpPhase(int32_t phaseHpPercent) {
	startSkillTask();
}

// Java UnstableRukrilAI.java:42-58 (the ebonsoul looked up once, kept alive by the task, as Java's capture does)
void UnstableRukrilAI::startSkillTask() {
	const runtime::Ref<Npc> ebonsoul = getPosition()->getWorldMapInstance()->getNpc(219552);
	skillTask.set(ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this, ebonsoul] {
		if (isDead())
			cancelTask();
		else {
			if (getPosition()->getWorldMapInstance()->getNpc(283204) == nullptr) {
				SkillEngine::getInstance().getSkill(getOwner(), 19266, 55, runtime::Ptr<VisibleObject>(getOwner()))->useNoAnimationSkill();
				spawn(283204, getOwner().getX() + 2, getOwner().getY() - 2, getOwner().getZ(), int8_t{0});
			}
			if (ebonsoul != nullptr && !ebonsoul->isDead()) {
				SkillEngine::getInstance().getSkill(*ebonsoul, 19159, 55, ebonsoul)->useNoAnimationSkill();
				spawn(283205, ebonsoul->getX() + 2, ebonsoul->getY() - 2, ebonsoul->getZ(), int8_t{0});
			}
		}
	}, 5000, 70000));
}

// Java UnstableRukrilAI.java:60-64
void UnstableRukrilAI::cancelTask() {
	runtime::FutureRef task = skillTask.get();
	if (task != nullptr && !task->isCancelled()) {
		task->cancel(true);
	}
}

// Java UnstableRukrilAI.java:66-72
void UnstableRukrilAI::tryRegen() {
	runtime::Ptr<Npc> ebonsoul = getPosition()->getWorldMapInstance()->getNpc(219552);
	if (ebonsoul != nullptr && !ebonsoul->isDead() && PositionUtil::isInRange(getOwner(), *ebonsoul, 5))
		if (!getOwner().getLifeStats()->isFullyRestoredHp())
			getOwner().getLifeStats()->increaseHp(SM_ATTACK_STATUS::TYPE::HP, 10000);
}

// Java UnstableRukrilAI.java:74-78
void UnstableRukrilAI::handleDied() {
	AggressiveNpcAI::handleDied();
	cancelTask();
}

// Java UnstableRukrilAI.java:80-86
void UnstableRukrilAI::handleBackHome() {
	AggressiveNpcAI::handleBackHome();
	cancelTask();
	hpPhases->reset();
	getEffectController()->removeEffect(19266);
}

// Java UnstableRukrilAI.java:88-92
void UnstableRukrilAI::handleDespawned() {
	cancelTask();
	AggressiveNpcAI::handleDespawned();
}

// Java UnstableRukrilAI.java:94-100
bool UnstableRukrilAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::REWARD_LOOT:
		case AIQuestion::REWARD_AP:
			return false;
		default:
			return AggressiveNpcAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::unstableSplinterpath
