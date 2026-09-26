#include "aion/gameserver/skillengine/condition/SkillChargeCondition.h"

namespace aion::gameserver::skillengine::condition {

bool SkillChargeCondition::validate(model::Skill& /*env*/) const {
	return true;
}

} // namespace aion::gameserver::skillengine::condition
