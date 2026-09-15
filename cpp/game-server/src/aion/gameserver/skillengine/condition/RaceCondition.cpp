#include "aion/gameserver/skillengine/condition/RaceCondition.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::skillengine::condition {

bool RaceCondition::validate(model::Skill& /*env*/) const {
	AION_UNPORTED();
}

bool RaceCondition::validate(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

} // namespace aion::gameserver::skillengine::condition
