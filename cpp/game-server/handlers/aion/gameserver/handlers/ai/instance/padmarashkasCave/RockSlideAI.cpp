#include "aion/gameserver/handlers/ai/instance/padmarashkasCave/RockSlideAI.h"

#include "aion/gameserver/ai/AIActions.h"

namespace aion::gameserver::handlers::ai::instance::padmarashkasCave {

AION_AI(RockSlideAI, "rock_slide");

// Java RockSlideAI.java:23-26
bool RockSlideAI::canThink() {
	return false;
}

// Java RockSlideAI.java:28-32
void RockSlideAI::handleSpawned() {
	AggressiveNpcAI::handleSpawned();
	ThreadPoolManager::getInstance().schedule({this}, [this] { useRockSlide(); }, 2000);
}

// Java RockSlideAI.java:34-37
float RockSlideAI::modifyDamage(Creature& attacker, float damage, runtime::Ptr<Effect> effect) {
	return 0;
}

// Java RockSlideAI.java:39-42
void RockSlideAI::useRockSlide() {
	AIActions::targetSelf(*this);
	AIActions::useSkill(*this, 19295);
}

// Java RockSlideAI.java:44-47
void RockSlideAI::onEndUseSkill(const skillengine::model::SkillTemplate* skillTemplate, int32_t skillLevel) {
	ThreadPoolManager::getInstance().schedule({this}, [this] { useRockSlide(); }, 2700);
}

} // namespace aion::gameserver::handlers::ai::instance::padmarashkasCave
