#include "aion/gameserver/skillengine/condition/HpCondition.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::skillengine::condition {

bool HpCondition::validate(model::Skill& /*env*/) const {
	AION_UNPORTED();
}

bool HpCondition::canValidate(model::Skill& /*skill*/) const {
	AION_UNPORTED();
}

} // namespace aion::gameserver::skillengine::condition
