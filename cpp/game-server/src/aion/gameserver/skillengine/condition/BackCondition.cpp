#include "aion/gameserver/skillengine/condition/BackCondition.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::skillengine::condition {

bool BackCondition::validate(model::Skill& /*env*/) const {
	AION_UNPORTED();
}

bool BackCondition::validate(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

} // namespace aion::gameserver::skillengine::condition
