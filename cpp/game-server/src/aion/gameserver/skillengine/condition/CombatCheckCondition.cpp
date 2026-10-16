#include "aion/gameserver/skillengine/condition/CombatCheckCondition.h"

#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/skillengine/model/Skill.h"

namespace aion::gameserver::skillengine::condition {

using gameserver::model::gameobjects::player::Player;
using runtime::Ptr;

bool CombatCheckCondition::validate(model::Skill& skill) const {
	if (Ptr<Player> player = runtime::as<Player>(skill.getEffector())) {
		return !player->getController().isInCombat();
	}
	return true;
}

} // namespace aion::gameserver::skillengine::condition
