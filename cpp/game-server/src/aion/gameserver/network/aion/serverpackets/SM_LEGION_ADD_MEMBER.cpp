#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_ADD_MEMBER.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_LEGION_ADD_MEMBER::SM_LEGION_ADD_MEMBER(model::gameobjects::player::Player& playerValue, bool isMemberValue, int32_t msgIdValue,
	std::string_view textValue)
	: AionServerPacket(opcodeOf<SM_LEGION_ADD_MEMBER>), player(playerValue), isMember(isMemberValue), msgId(msgIdValue), text(textValue) {
}

SM_LEGION_ADD_MEMBER::~SM_LEGION_ADD_MEMBER() = default;

void SM_LEGION_ADD_MEMBER::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
