#include "aion/gameserver/handlers/ai/instance/rakes/WalkingTalkingBombAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"

namespace aion::gameserver::handlers::ai::instance::rakes {

AION_AI(WalkingTalkingBombAI, "walkingtalkingbomb");

// Java WalkingTalkingBombAI.java:19-23
void WalkingTalkingBombAI::handleSpawned() {
	AggressiveNpcAI::handleSpawned();
	doSchedule();
}

// Java WalkingTalkingBombAI.java:25-35
void WalkingTalkingBombAI::doSchedule() {
	ThreadPoolManager::getInstance().schedule({this}, [this] {
		if (!isDead()) {
			SkillEngine::getInstance().getSkill(getOwner(), 19416, 49, runtime::Ptr<VisibleObject>(getOwner()))->useNoAnimationSkill();
			ThreadPoolManager::getInstance().schedule({this}, [this] {
				if (!isDead())
					despawn();
			}, 4000);
		}
	}, 2000);
}

// Java WalkingTalkingBombAI.java:37-44
bool WalkingTalkingBombAI::ask(AIQuestion question) {
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

// Java WalkingTalkingBombAI.java:46-48
void WalkingTalkingBombAI::despawn() {
	AIActions::deleteOwner(*this);
}

} // namespace aion::gameserver::handlers::ai::instance::rakes
