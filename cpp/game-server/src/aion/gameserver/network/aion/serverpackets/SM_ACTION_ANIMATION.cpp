#include "aion/gameserver/network/aion/serverpackets/SM_ACTION_ANIMATION.h"

#include "aion/gameserver/model/animations/ActionAnimationInfo.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_ACTION_ANIMATION::SM_ACTION_ANIMATION(int32_t targetObjectIdValue, model::animations::ActionAnimation actionAnimationValue)
	: SM_ACTION_ANIMATION(targetObjectIdValue, actionAnimationValue, 0) {
}

SM_ACTION_ANIMATION::SM_ACTION_ANIMATION(int32_t targetObjectIdValue, model::animations::ActionAnimation actionAnimationValue,
	int32_t levelOrObjectIdValue)
	: AionServerPacket(opcodeOf<SM_ACTION_ANIMATION>), targetObjectId(targetObjectIdValue), actionAnimation(actionAnimationValue),
	  levelOrObjectId(levelOrObjectIdValue) {
}

void SM_ACTION_ANIMATION::writeImpl(AionConnection* con) {
	writeD(targetObjectId);
	writeH(model::animations::getId(actionAnimation));
	writeD(levelOrObjectId);
}

} // namespace aion::gameserver::network::aion::serverpackets
