#include "aion/gameserver/network/aion/serverpackets/SM_ENTER_WORLD_CHECK.h"

#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ENTER_WORLD_CHECK_MsgInfo.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_ENTER_WORLD_CHECK::SM_ENTER_WORLD_CHECK(SM_ENTER_WORLD_CHECK::Msg msgValue) : AionServerPacket(opcodeOf<SM_ENTER_WORLD_CHECK>), msg(getId(msgValue)) {
}

SM_ENTER_WORLD_CHECK::SM_ENTER_WORLD_CHECK() : SM_ENTER_WORLD_CHECK(SM_ENTER_WORLD_CHECK::Msg::OK) {
}

void SM_ENTER_WORLD_CHECK::writeImpl(AionConnection* con) {
	writeC(msg);
	writeC(0x00);
	writeC(0x00);
}

} // namespace aion::gameserver::network::aion::serverpackets
