#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_LEAVE_MEMBER.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_LEGION_LEAVE_MEMBER::SM_LEGION_LEAVE_MEMBER(int32_t msgIdValue, int32_t playerObjIdValue, std::string_view nameValue)
	: AionServerPacket(opcodeOf<SM_LEGION_LEAVE_MEMBER>), name(nameValue), playerObjId(playerObjIdValue), msgId(msgIdValue) {
}

SM_LEGION_LEAVE_MEMBER::SM_LEGION_LEAVE_MEMBER(int32_t msgIdValue, int32_t playerObjIdValue, std::string_view nameValue, std::string_view name1Value)
	: AionServerPacket(opcodeOf<SM_LEGION_LEAVE_MEMBER>), name(nameValue), name1(name1Value), playerObjId(playerObjIdValue), msgId(msgIdValue) {
}

void SM_LEGION_LEAVE_MEMBER::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
