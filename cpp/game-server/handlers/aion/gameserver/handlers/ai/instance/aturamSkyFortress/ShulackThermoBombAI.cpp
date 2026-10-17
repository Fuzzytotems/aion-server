#include "aion/gameserver/handlers/ai/instance/aturamSkyFortress/ShulackThermoBombAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"

namespace aion::gameserver::handlers::ai::instance::aturamSkyFortress {

AION_AI(ShulackThermoBombAI, "shulack_thermo_bomb");

// Java ShulackThermoBombAI.java:22-26
void ShulackThermoBombAI::handleSpawned() {
	AggressiveNpcAI::handleSpawned();
	doSchedule();
}

// Java ShulackThermoBombAI.java:28-40
void ShulackThermoBombAI::doSchedule() {
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

// Java ShulackThermoBombAI.java:42-49
bool ShulackThermoBombAI::ask(AIQuestion question) {
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

// Java ShulackThermoBombAI.java:51-53
void ShulackThermoBombAI::despawn() {
	AIActions::deleteOwner(*this);
}

} // namespace aion::gameserver::handlers::ai::instance::aturamSkyFortress
