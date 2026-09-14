#include "aion/gameserver/network/aion/serverpackets/SM_PACKAGE_INFO_NOTIFY.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_PACKAGE_INFO_NOTIFY::SM_PACKAGE_INFO_NOTIFY()
	: AionServerPacket(opcodeOf<SM_PACKAGE_INFO_NOTIFY>) {
}

void SM_PACKAGE_INFO_NOTIFY::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
