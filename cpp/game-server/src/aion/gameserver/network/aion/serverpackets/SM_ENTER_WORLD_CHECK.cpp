#include "aion/gameserver/network/aion/serverpackets/SM_ENTER_WORLD_CHECK.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_ENTER_WORLD_CHECK::SM_ENTER_WORLD_CHECK(SM_ENTER_WORLD_CHECK::Msg msgValue) : AionServerPacket(opcodeOf<SM_ENTER_WORLD_CHECK>) {
	AION_UNPORTED();
}

SM_ENTER_WORLD_CHECK::SM_ENTER_WORLD_CHECK() : SM_ENTER_WORLD_CHECK(SM_ENTER_WORLD_CHECK::Msg::OK) {
}

void SM_ENTER_WORLD_CHECK::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
