#include "aion/gameserver/network/aion/serverpackets/SM_ACCOUNT_PROPERTIES.h"

#include "aion/gameserver/configs/administration/AdminConfig.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::serverpackets {

SM_ACCOUNT_PROPERTIES::SM_ACCOUNT_PROPERTIES() : AionServerPacket(opcodeOf<SM_ACCOUNT_PROPERTIES>) {
}

void SM_ACCOUNT_PROPERTIES::writeImpl(AionConnection* con) {
	if (con == nullptr)
		throw runtime::NullPointerException("SM_ACCOUNT_PROPERTIES::writeImpl without a connection");
	// enables GM panel and other windows, also disables client-side faction restriction for char creation
	writeC(con->getAccount()->getAccessLevel() >= configs::administration::AdminConfig::GM_PANEL.load() ? 1 : 0);
	writeD(0); // always 0
	writeH(0); // 0 or 52168 (C8 CB)
	writeC(0); // 0 or 1
	writeH(0); // 0, 4 or 5
	writeC(0); // 0, 4 or 124
	writeH(0); // can be 1
	writeD(0); // 0 or 16 (or 31 = strong energy of repose)
	writeD(0); // always 0
	writeD(0); // purchased packet (8 = gold pack)
	writeD(4); // account status (0 = gold-user, 1/2 = starter, 3/4 = veteran)
}

} // namespace aion::gameserver::network::aion::serverpackets
