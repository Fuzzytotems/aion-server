#include "aion/gameserver/skillengine/condition/FrontCondition.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::skillengine::condition {

bool FrontCondition::validate(model::Skill& /*env*/) const {
	AION_UNPORTED();
}

bool FrontCondition::validate(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

} // namespace aion::gameserver::skillengine::condition
