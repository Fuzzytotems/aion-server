#include "aion/gameserver/network/aion/serverpackets/SM_L2AUTH_LOGIN_CHECK.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_L2AUTH_LOGIN_CHECK::SM_L2AUTH_LOGIN_CHECK(bool okValue, std::string_view accountNameValue)
	: AionServerPacket(opcodeOf<SM_L2AUTH_LOGIN_CHECK>), ok(okValue), accountName(accountNameValue) {
}

void SM_L2AUTH_LOGIN_CHECK::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
