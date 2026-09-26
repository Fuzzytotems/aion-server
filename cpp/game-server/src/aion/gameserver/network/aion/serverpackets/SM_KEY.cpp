#include "aion/gameserver/network/aion/serverpackets/SM_KEY.h"

#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::serverpackets {

SM_KEY::SM_KEY() : AionServerPacket(opcodeOf<SM_KEY>) {
}

void SM_KEY::writeImpl(AionConnection* con) {
	if (con == nullptr)
		throw runtime::NullPointerException("SM_KEY::writeImpl without a connection");
	writeD(con->enableCryptKey());
}

} // namespace aion::gameserver::network::aion::serverpackets
