#include "aion/gameserver/skillengine/condition/DpCondition.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/skillengine/model/Skill.h"

namespace aion::gameserver::skillengine::condition {

using gameserver::model::gameobjects::player::Player;

bool DpCondition::validate(model::Skill& skill) const {
	// Java: ((Player) skill.getEffector()) - a ClassCastException for any other effector, which runtime::cast throws too
	return runtime::cast<Player>(skill.getEffector())->getCommonData()->getDp() >= value;
}

} // namespace aion::gameserver::skillengine::condition
