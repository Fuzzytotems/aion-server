#include "aion/gameserver/network/aion/serverpackets/SM_PACKAGE_INFO_NOTIFY.h"

#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_PACKAGE_INFO_NOTIFY::SM_PACKAGE_INFO_NOTIFY()
	: AionServerPacket(opcodeOf<SM_PACKAGE_INFO_NOTIFY>) {
}

void SM_PACKAGE_INFO_NOTIFY::writeImpl(AionConnection* con) {
	writeH(1);
	writeC(3);
	writeD(0); // time until pack expiration
}

} // namespace aion::gameserver::network::aion::serverpackets
