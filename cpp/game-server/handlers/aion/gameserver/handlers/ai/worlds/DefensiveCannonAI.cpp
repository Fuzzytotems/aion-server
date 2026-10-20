#include "aion/gameserver/handlers/ai/worlds/DefensiveCannonAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"

namespace aion::gameserver::handlers::ai::worlds {

AION_AI(DefensiveCannonAI, "defensive_cannon");

// Java DefensiveCannonAI.java:25-35
void DefensiveCannonAI::handleUseItemFinish(Player& player) {
	if (canUse.compareAndSet(true, false)) {
		switch (getNpcId()) {
			case 831338:
			case 831339: {
				runtime::Ref<skillengine::model::Skill> skill = SkillEngine::getInstance().getSkill(getOwner(), 20364, 60, runtime::Ptr<VisibleObject>(player));
				if (skill == nullptr) // Java: getSkill(...).useNoAnimationSkill() on null
					throw runtime::NullPointerException("DefensiveCannonAI: skill 20364");
				skill->useNoAnimationSkill(); // Board Artillery Morph
				break;
			}
			default:
				break;
		}
		AIActions::deleteOwner(*this);
	}
}

// Java DefensiveCannonAI.java:37-43
bool DefensiveCannonAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::REWARD_AP_XP_DP_LOOT:
			return false;
		default:
			return ActionItemNpcAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai::worlds
