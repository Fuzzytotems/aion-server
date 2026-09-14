#include "aion/gameserver/network/aion/serverpackets/SM_GROUP_MEMBER_INFO.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_GROUP_MEMBER_INFO::SM_GROUP_MEMBER_INFO(model::team::group::PlayerGroup& group, model::gameobjects::player::Player& playerValue,
	model::team::common::legacy::GroupEvent eventValue, int32_t slotValue)
	: AionServerPacket(opcodeOf<SM_GROUP_MEMBER_INFO>) {
	AION_UNPORTED();
}

SM_GROUP_MEMBER_INFO::SM_GROUP_MEMBER_INFO(model::team::group::PlayerGroup& group, model::gameobjects::player::Player& playerValue,
	model::team::common::legacy::GroupEvent eventValue)
	: SM_GROUP_MEMBER_INFO(group, playerValue, eventValue, 0) {
}

SM_GROUP_MEMBER_INFO::~SM_GROUP_MEMBER_INFO() = default;

void SM_GROUP_MEMBER_INFO::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
