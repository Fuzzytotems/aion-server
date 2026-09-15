#include "aion/gameserver/model/gameobjects/player/motion/Motion.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/motion/MotionList.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::model::gameobjects::player::motion {

Motion::Motion(int32_t idValue, int32_t deletionTimeValue, bool isActive) : id(idValue), deletionTime(deletionTimeValue), active(isActive) {
}

Motion::~Motion() = default;

runtime::Ref<Motion> Motion::create(int32_t idValue, int32_t deletionTimeValue, bool isActive) {
	return runtime::makeRef<Motion>(idValue, deletionTimeValue, isActive);
}

void Motion::onExpire(Player& player) {
	player.getMotions().remove(id);
	// TODO motion templates -> parse nameIds for system message, like 600533 for STR_CMOTION_CASH_NINJA_IDLE (Ninja Idle) etc.
	utils::PacketSendUtility::sendPacket(player,
		network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_DELETE_CASH_CUSTOMANIMATION_BY_TIMEOUT(/* nameId */));
}

} // namespace aion::gameserver::model::gameobjects::player::motion
