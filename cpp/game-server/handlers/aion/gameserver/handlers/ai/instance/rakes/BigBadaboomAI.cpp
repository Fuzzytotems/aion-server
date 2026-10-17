#include "aion/gameserver/handlers/ai/instance/rakes/BigBadaboomAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"

namespace aion::gameserver::handlers::ai::instance::rakes {

AION_AI(BigBadaboomAI, "big_badaboom");

// Java BigBadaboomAI.java:18-29
void BigBadaboomAI::handleUseItemFinish(Player& player) {
	int32_t morphSkill = 0;
	switch (getNpcId()) {
		case 231016: // Big Badaboom.
		case 231017: // Bigger Badaboom.
			morphSkill = 0x4E502E;
			break;
		default:
			break;
	}
	SkillEngine::getInstance().getSkill(getOwner(), morphSkill >> 8, morphSkill & 0xFF, runtime::Ptr<VisibleObject>(player))->useNoAnimationSkill();
	AIActions::deleteOwner(*this);
}

} // namespace aion::gameserver::handlers::ai::instance::rakes
