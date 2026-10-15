#include "aion/gameserver/handlers/ai/instance/rentusBase/SpilledOilAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"

namespace aion::gameserver::handlers::ai::instance::rentusBase {

AION_AI(SpilledOilAI, "spilled_oil");

// Java SpilledOilAI.java:23-27
void SpilledOilAI::handleSpawned() {
	GeneralNpcAI::handleSpawned();
	startEventTask();
}

// Java SpilledOilAI.java:29-47 (the anonymous Runnable: a lambda pinned to the AI)
void SpilledOilAI::startEventTask() {
	ThreadPoolManager::getInstance().schedule({this}, [this] {
		if (!isDead()) {
			count = count + 1;
			if (count.get() < 7) {
				SkillEngine::getInstance().getSkill(getOwner(), 19658, 60, runtime::Ptr<VisibleObject>(getOwner()))->useNoAnimationSkill();
				startEventTask();
			} else {
				delete_();
			}
		}

	}, 4000);
}

// Java SpilledOilAI.java:49-51 (delete: a C++ keyword)
void SpilledOilAI::delete_() {
	AIActions::deleteOwner(*this);
}

} // namespace aion::gameserver::handlers::ai::instance::rentusBase
