#include "aion/gameserver/skillengine/condition/AbnormalStateCondition.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::skillengine::condition {

bool AbnormalStateCondition::validate(model::Skill& /*env*/) const {
	AION_UNPORTED();
}

bool AbnormalStateCondition::validate(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

} // namespace aion::gameserver::skillengine::condition
