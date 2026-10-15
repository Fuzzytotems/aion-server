#include "aion/gameserver/handlers/ai/instance/raksang/RaksangRubbleAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"

namespace aion::gameserver::handlers::ai::instance::raksang {

AION_AI(RaksangRubbleAI, "raksang_rubble");

// Java RaksangRubbleAI.java:22-25
bool RaksangRubbleAI::canThink() {
	return false;
}

// Java RaksangRubbleAI.java:27-34
void RaksangRubbleAI::handleSpawned() {
	AggressiveNpcAI::handleSpawned();
	ThreadPoolManager::getInstance().schedule({this}, [this] {
		SkillEngine::getInstance().getSkill(getOwner(), 19937, 46, runtime::Ptr<VisibleObject>(getOwner()))->useNoAnimationSkill();
		startLifeTask();
	}, 1000);
}

// Java RaksangRubbleAI.java:36-38
void RaksangRubbleAI::startLifeTask() {
	ThreadPoolManager::getInstance().schedule({this}, [this] { AIActions::deleteOwner(*this); }, 9000);
}

// Java RaksangRubbleAI.java:40-46
bool RaksangRubbleAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::IS_IMMUNE_TO_ABNORMAL_STATES:
			return true;
		default:
			return AggressiveNpcAI::ask(question);
	}
}

// Java RaksangRubbleAI.java:48-52
void RaksangRubbleAI::handleDied() {
	AggressiveNpcAI::handleDied();
	AIActions::deleteOwner(*this);
}

} // namespace aion::gameserver::handlers::ai::instance::raksang
