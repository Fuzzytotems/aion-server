#include "aion/gameserver/handlers/ai/instance/empyreanCrucible/SparkOfDarknessAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"

namespace aion::gameserver::handlers::ai::instance::empyreanCrucible {

AION_AI(SparkOfDarknessAI, "spark_of_darkness");

// Java SparkOfDarknessAI.java:21-26
void SparkOfDarknessAI::handleSpawned() {
	GeneralNpcAI::handleSpawned();
	startEventTask();
	startLifeTask();
}

// Java SparkOfDarknessAI.java:28-33
void SparkOfDarknessAI::startEventTask() {
	ThreadPoolManager::getInstance().schedule({this}, [this] {
		if (!isDead())
			skillengine::SkillEngine::getInstance().getSkill(getOwner(), 19554, 1, runtime::Ptr<VisibleObject>(getOwner()))->useNoAnimationSkill();
	}, 500);
}

// Java SparkOfDarknessAI.java:35-37
void SparkOfDarknessAI::startLifeTask() {
	ThreadPoolManager::getInstance().schedule({this}, [this] { AIActions::deleteOwner(*this); }, 6500);
}

} // namespace aion::gameserver::handlers::ai::instance::empyreanCrucible
