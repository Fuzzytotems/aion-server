#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_LEAVE_MEMBER.h"

#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_LEGION_LEAVE_MEMBER::SM_LEGION_LEAVE_MEMBER(int32_t msgIdValue, int32_t playerObjIdValue, std::string_view nameValue)
	: AionServerPacket(opcodeOf<SM_LEGION_LEAVE_MEMBER>), name(nameValue), playerObjId(playerObjIdValue), msgId(msgIdValue) {
}

SM_LEGION_LEAVE_MEMBER::SM_LEGION_LEAVE_MEMBER(int32_t msgIdValue, int32_t playerObjIdValue, std::string_view nameValue, std::string_view name1Value)
	: AionServerPacket(opcodeOf<SM_LEGION_LEAVE_MEMBER>), name(nameValue), name1(name1Value), playerObjId(playerObjIdValue), msgId(msgIdValue) {
}

void SM_LEGION_LEAVE_MEMBER::writeImpl(AionConnection* con) {
	writeD(playerObjId);
	writeC(0x00); // isMember ? 1 : 0
	writeD(0x00); // unix time for log off
	writeD(msgId);
	writeS(name);
	writeS(name1); // Java null (three-argument constructor): writeS writes only the terminator, like ""
}

} // namespace aion::gameserver::network::aion::serverpackets
