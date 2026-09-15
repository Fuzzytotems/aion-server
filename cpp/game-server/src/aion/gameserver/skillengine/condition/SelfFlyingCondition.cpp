#include "aion/gameserver/skillengine/condition/SelfFlyingCondition.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::skillengine::condition {

bool SelfFlyingCondition::validate(model::Skill& /*env*/) const {
	AION_UNPORTED();
}

bool SelfFlyingCondition::validate(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

} // namespace aion::gameserver::skillengine::condition
