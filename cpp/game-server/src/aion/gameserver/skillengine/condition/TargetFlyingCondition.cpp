#include "aion/gameserver/skillengine/condition/TargetFlyingCondition.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::skillengine::condition {

bool TargetFlyingCondition::validate(model::Skill& /*env*/) const {
	AION_UNPORTED();
}

bool TargetFlyingCondition::validate(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

} // namespace aion::gameserver::skillengine::condition
