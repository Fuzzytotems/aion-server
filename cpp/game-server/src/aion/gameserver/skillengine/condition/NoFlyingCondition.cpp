#include "aion/gameserver/skillengine/condition/NoFlyingCondition.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::skillengine::condition {

bool NoFlyingCondition::validate(model::Skill& /*env*/) const {
	AION_UNPORTED();
}

bool NoFlyingCondition::validate(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

} // namespace aion::gameserver::skillengine::condition
