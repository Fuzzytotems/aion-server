#include "aion/gameserver/skillengine/condition/MpCondition.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::skillengine::condition {

bool MpCondition::validate(model::Skill& /*env*/) const {
	AION_UNPORTED();
}

bool MpCondition::canValidate(model::Skill& /*skill*/) const {
	AION_UNPORTED();
}

} // namespace aion::gameserver::skillengine::condition
