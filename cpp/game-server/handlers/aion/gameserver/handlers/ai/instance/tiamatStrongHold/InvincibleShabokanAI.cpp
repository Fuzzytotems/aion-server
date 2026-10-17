#include "aion/gameserver/handlers/ai/instance/tiamatStrongHold/InvincibleShabokanAI.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold {

AION_AI(InvincibleShabokanAI, "invincibleshabokan");

// Java InvincibleShabokanAI.java:32-41
void InvincibleShabokanAI::handleAttack(runtime::Ptr<Creature> creature) {
	AggressiveNpcAI::handleAttack(creature);
	if (isHome.compareAndSet(true, false))
		startSkillTask();
	if (!isFinalBuff.get() && getOwner().getLifeStats()->getHpPercentage() <= 25) {
		isFinalBuff = true;
		AIActions::useSkill(*this, 20941);
	}
}

// Java InvincibleShabokanAI.java:43-55 (the anonymous Runnable: a task pinning this)
void InvincibleShabokanAI::startSkillTask() {
	skillTask.set(ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] {
		if (isDead())
			cancelTask();
		else {
			chooseRandomEvent();
		}
	}, 5000, 30000));
}

// Java InvincibleShabokanAI.java:57-61
void InvincibleShabokanAI::cancelTask() {
	runtime::FutureRef task = skillTask.get();
	if (task != nullptr && !task->isCancelled()) {
		task->cancel(true);
	}
}

// Java InvincibleShabokanAI.java:63-69
void InvincibleShabokanAI::earthQuakeEvent() {
	runtime::Ptr<Npc> invisible = getPosition()->getWorldMapInstance()->getNpc(283082); // 4.0
	SkillEngine::getInstance().getSkill(getOwner(), 20717, 55, runtime::Ptr<VisibleObject>(getOwner()))->useNoAnimationSkill();
	if (invisible == nullptr) {
		spawn(283082, getOwner().getX(), getOwner().getY(), getOwner().getZ(), int8_t{0}); // 4.0
	}
}

// Java InvincibleShabokanAI.java:71-79
void InvincibleShabokanAI::sinkEvent() {
	SkillEngine::getInstance().getSkill(getOwner(), 20720, 55, runtime::Ptr<VisibleObject>(getOwner()))->useNoAnimationSkill();
	for (const runtime::Ptr<Player>& player : getKnownList().streamVisiblePlayers()) {
		if (isInRange(*player, 30)) {
			spawn(283083, player->getX(), player->getY(), player->getZ(), int8_t{0}); // 4.0
			spawn(283084, player->getX(), player->getY(), player->getZ(), int8_t{0}); // 4.0
		}
	}
}

// Java InvincibleShabokanAI.java:81-87
void InvincibleShabokanAI::chooseRandomEvent() {
	int32_t rand = commons::utils::Rnd::get(0, 1);
	if (rand == 0)
		earthQuakeEvent();
	else
		sinkEvent();
}

// Java InvincibleShabokanAI.java:89-93
void InvincibleShabokanAI::handleDied() {
	AggressiveNpcAI::handleDied();
	cancelTask();
}

// Java InvincibleShabokanAI.java:95-99
void InvincibleShabokanAI::handleDespawned() {
	AggressiveNpcAI::handleDespawned();
	cancelTask();
}

// Java InvincibleShabokanAI.java:101-107
void InvincibleShabokanAI::handleBackHome() {
	AggressiveNpcAI::handleBackHome();
	cancelTask();
	getOwner().getEffectController()->removeEffect(20941);
	isHome.set(true);
}

} // namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold
