#include "aion/gameserver/handlers/ai/instance/aturamSkyFortress/ShulackGuidedBombAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/utils/PositionUtil.h"

namespace aion::gameserver::handlers::ai::instance::aturamSkyFortress {

AION_AI(ShulackGuidedBombAI, "shulack_guided_bomb");

// Java ShulackGuidedBombAI.java:30-35
void ShulackGuidedBombAI::handleDespawned() {
	AggressiveNpcAI::handleDespawned();
	if (runtime::FutureRef t = task.get(); t != nullptr)
		t->cancel(true);
}

// Java ShulackGuidedBombAI.java:37-41
void ShulackGuidedBombAI::handleSpawned() {
	AggressiveNpcAI::handleSpawned();
	starLifeTask();
}

// Java ShulackGuidedBombAI.java:43-50
void ShulackGuidedBombAI::handleCreatureAggro(Creature& creature) {
	AggressiveNpcAI::handleCreatureAggro(creature);
	if (isHome.get()) {
		isHome = false;
		doSchedule(creature);
	}
}

// Java ShulackGuidedBombAI.java:52-57
void ShulackGuidedBombAI::starLifeTask() {
	ThreadPoolManager::getInstance().schedule({this}, [this] {
		if (!isDead() && !isDestroyed.get())
			despawn();
	}, 10000);
}

// Java ShulackGuidedBombAI.java:59-71
void ShulackGuidedBombAI::doSchedule(Creature& creature) {
	runtime::Ref<Creature> target(creature);
	task.set(ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this, target] {
		if (!isDead() && !isDestroyed.get()) {
			destroy(runtime::Ptr<Creature>(target));
		} else {
			if (runtime::FutureRef t = task.get(); t != nullptr) {
				t->cancel(true);
			}
		}
	}, 1000, 1000));
}

// Java ShulackGuidedBombAI.java:73-77
void ShulackGuidedBombAI::despawn() {
	if (!isDead()) {
		AIActions::deleteOwner(*this);
	}
}

// Java ShulackGuidedBombAI.java:79-87
void ShulackGuidedBombAI::destroy(runtime::Ptr<Creature> creature) {
	if (!isDestroyed.get() && !isDead()) {
		if (creature != nullptr && PositionUtil::getDistance(getOwner(), *creature) <= 4) {
			isDestroyed = true;
			SkillEngine::getInstance().getSkill(getOwner(), 19415, 49, runtime::Ptr<VisibleObject>(getOwner()))->useNoAnimationSkill();
			ThreadPoolManager::getInstance().schedule({this}, [this] { despawn(); }, 3200);
		}
	}
}

// Java ShulackGuidedBombAI.java:89-96
bool ShulackGuidedBombAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::ALLOW_DECAY:
		case AIQuestion::ALLOW_RESPAWN:
		case AIQuestion::REWARD_AP_XP_DP_LOOT:
			return false;
		case AIQuestion::IS_IMMUNE_TO_ABNORMAL_STATES:
			return true;
		default:
			return AggressiveNpcAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::aturamSkyFortress
