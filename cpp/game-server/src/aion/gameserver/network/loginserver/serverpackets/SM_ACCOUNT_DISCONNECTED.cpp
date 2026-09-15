#include "aion/gameserver/network/loginserver/serverpackets/SM_ACCOUNT_DISCONNECTED.h"

namespace aion::gameserver::network::loginserver::serverpackets {

SM_ACCOUNT_DISCONNECTED::SM_ACCOUNT_DISCONNECTED(int32_t accountIdValue) : LsServerPacket(0x03), accountId(accountIdValue) {
}

void SM_ACCOUNT_DISCONNECTED::writeImpl(LoginServerConnection* con, commons::utils::ByteBuffer& buf) {
	writeD(buf, accountId);
}

} // namespace aion::gameserver::network::loginserver::serverpackets
