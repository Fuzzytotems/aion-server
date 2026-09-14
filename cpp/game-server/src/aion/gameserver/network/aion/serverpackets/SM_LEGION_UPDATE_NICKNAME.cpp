#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_UPDATE_NICKNAME.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_LEGION_UPDATE_NICKNAME::SM_LEGION_UPDATE_NICKNAME(int32_t playerObjIdValue, std::string_view newNicknameValue)
	: AionServerPacket(opcodeOf<SM_LEGION_UPDATE_NICKNAME>), playerObjId(playerObjIdValue), newNickname(newNicknameValue) {
}

void SM_LEGION_UPDATE_NICKNAME::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
