#include "aion/gameserver/skillengine/condition/FormCondition.h"

#include "aion/gameserver/model/gameobjects/TransformModel.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::skillengine::condition {

using gameserver::model::gameobjects::player::Player;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using runtime::Ptr;

bool FormCondition::validate(model::Skill& env) const {
	if (Ptr<Player> player = runtime::as<Player>(env.getEffector())) {
		if (env.getEffector()->getTransformModel().isActive() && env.getEffector()->getTransformModel().getType() == value)
			return true;
		else {
			utils::PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_SKILL_CAN_NOT_CAST_IN_THIS_FORM());
			return false;
		}
	} else
		return true;
}

} // namespace aion::gameserver::skillengine::condition
