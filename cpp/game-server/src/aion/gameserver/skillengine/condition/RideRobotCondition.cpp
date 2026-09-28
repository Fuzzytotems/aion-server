#include "aion/gameserver/skillengine/condition/RideRobotCondition.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/skillengine/model/Skill.h"

namespace aion::gameserver::skillengine::condition {

using gameserver::model::gameobjects::player::Player;
using runtime::Ptr;

bool RideRobotCondition::validate(model::Skill& skill) const {
	if (Ptr<Player> player = runtime::as<Player>(skill.getEffector())) {
		return player->isInRobotMode();
	} else {
		return true;
	}
}

} // namespace aion::gameserver::skillengine::condition
