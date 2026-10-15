#include "aion/gameserver/handlers/ai/instance/raksang/RaksangGargoyleAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"

namespace aion::gameserver::handlers::ai::instance::raksang {

AION_AI(RaksangGargoyleAI, "raksang_gargoyle");

// Java RaksangGargoyleAI.java:22-29
void RaksangGargoyleAI::handleSpawned() {
	AggressiveNpcAI::handleSpawned();
	ThreadPoolManager::getInstance().schedule({this}, [this] {
		if (!isDead())
			SkillEngine::getInstance().getSkill(getOwner(), 19126, 46, runtime::Ptr<VisibleObject>(getOwner()))->useNoAnimationSkill();
	}, 2000);
}

// Java RaksangGargoyleAI.java:31-37
bool RaksangGargoyleAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::IS_IMMUNE_TO_ABNORMAL_STATES:
			return true;
		default:
			return AggressiveNpcAI::ask(question);
	}
}

// Java RaksangGargoyleAI.java:39-43
void RaksangGargoyleAI::handleDied() {
	AggressiveNpcAI::handleDied();
	AIActions::deleteOwner(*this);
}

} // namespace aion::gameserver::handlers::ai::instance::raksang
