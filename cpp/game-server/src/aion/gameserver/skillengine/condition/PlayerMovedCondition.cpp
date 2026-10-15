#include "aion/gameserver/skillengine/condition/PlayerMovedCondition.h"

#include "aion/gameserver/controllers/observer/StartMovingListener.h"
#include "aion/gameserver/skillengine/model/Skill.h"

namespace aion::gameserver::skillengine::condition {

bool PlayerMovedCondition::validate(model::Skill& skill) const {
	return allow == skill.getMoveListener()->isEffectorMoved();
}

} // namespace aion::gameserver::skillengine::condition
